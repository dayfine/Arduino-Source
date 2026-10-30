/*  BDSP AutoStory - Game Settings
 *  Checks the game's options at the start of a new game, and sets the
 *  Switch's clock.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Programs/DateSpam/NintendoSwitch_HomeToDateTime.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "PokemonBDSP/Inference/PokemonBDSP_TransitionWatchers.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"

#include "PokemonBDSP_AutoStory_GameSettings.h"

using namespace std::chrono_literals;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // ---------------------------------------------------------------------------
            // Time helper
            // ---------------------------------------------------------------------------

            // Options chrome and position detectors are independent of setting values.
            class TutorialOptionsDetector {
            public:
                bool detect(const ImageViewRGB32& screen) const {
                    ImageViewRGB32 panel = extract_box_reference(screen, ImageFloatBox{0.04, 0.065, 0.93, 0.70});
                    size_t white = 0;
                    for (size_t y = 0; y < panel.height(); y += 4)
                        for (size_t x = 0; x < panel.width(); x += 4) {
                            uint32_t p = panel.pixel(x, y);
                            if (((p >> 16) & 255) > 225 && ((p >> 8) & 255) > 225 && (p & 255) > 225) white++;
                        }
                    double samples = (double)(((panel.width() + 3) / 4) * ((panel.height() + 3) / 4));
                    // Distinguish Options from the icon menu and bag using its
                    // blue heading and full-width white settings panel.
                    ImageViewRGB32 heading = extract_box_reference(screen, ImageFloatBox{0.01, 0.005, 0.20, 0.025});
                    size_t blue = 0, total = 0;
                    for (size_t y = 0; y < heading.height(); y++)
                        for (size_t x = 0; x < heading.width(); x++) {
                            uint32_t p = heading.pixel(x, y);
                            int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                            blue += b > r + 20 && g > r + 10;
                            total++;
                        }
                    return samples > 0 && white / samples > 0.70 && total > 0 && blue > total / 4;
                }
            };

            // Options fades in after A on its icon: poll instead of judging one frame.
            static bool wait_for_options_screen(VideoStream& stream, ProControllerContext& context,
                const TutorialOptionsDetector& options_screen, Milliseconds timeout) {
                context.wait_for_all_requests();
                const WallClock deadline = current_time() + timeout;
                while (current_time() < deadline) {
                    auto frame = stream.video().snapshot();
                    if (frame && options_screen.detect(frame)) return true;
                    context.wait_for(100ms);
                }
                return false;
            }

            class TutorialOptionPositionDetector {
            public:
                // Locate the two long red horizontal lines surrounding the
                // selected row. Scrolling changes their position automatically.
                double selected_row(const ImageViewRGB32& screen) const {
                    int first = -1, last = -1;
                    for (size_t y = (size_t)(screen.height() * 0.08); y < screen.height() * 0.75; y++) {
                        size_t red = 0, total = 0;
                        for (size_t x = (size_t)(screen.width() * 0.08); x < screen.width() * 0.90; x += 2) {
                            uint32_t p = screen.pixel(x, y);
                            int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                            red += r > 150 && r > g + 40 && r > b + 40;
                            total++;
                        }
                        if (total && red > total * 0.70) {
                            if (first < 0) first = (int)y;
                            last = (int)y;
                        }
                    }
                    if (first < 0 || last - first < screen.height() * 0.04 || last - first > screen.height() * 0.12) return -1;
                    return (first + last) / (2.0 * screen.height());
                }
                // Locate selected red value text separately from the row border.
                bool selected_value(const ImageViewRGB32& screen, double y) const {
                    ImageViewRGB32 value = extract_box_reference(screen, ImageFloatBox{0.33, y - 0.025, 0.54, 0.05});
                    size_t red = 0;
                    for (size_t py = 0; py < value.height(); py++)
                        for (size_t px = 0; px < value.width(); px++) {
                            uint32_t p = value.pixel(px, py);
                            int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                            red += r > g + 20 && r > b + 20;
                        }
                    return red > value.width() * value.height() * 0.0005;
                }
            };


            // Compare binary visual shapes with small alignment tolerance.
            // No text recognition is needed for fixed English option labels.
            static double option_image_score(const ImageViewRGB32& screen,
                const ImageFloatBox& box, const ImageRGB32& reference, bool red_only) {
                ImageRGB32 actual = extract_box_reference(screen, box).scale_to(384, 32);
                ImageRGB32 expected = reference.scale_to(384, 32);
                auto ink = [red_only](uint32_t p){
                    int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                    return red_only ? r > g + 20 && r > b + 20 : r + g + b < 500;
                };
                double best = 0;
                for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
                    size_t overlap = 0, count = 0;
                    for (int y = 2; y < 30; y++) for (int x = 2; x < 382; x++) {
                        bool a = ink(actual.pixel(x + dx, y + dy));
                        bool b = ink(expected.pixel(x, y));
                        overlap += a && b;
                        count += a + b;
                    }
                    if (count) best = std::max(best, 2.0 * overlap / count);
                }
                return best;
            }

            static bool autosave_off_highlight(const ImageViewRGB32& screen, double y) {
                auto red_count = [&](const ImageFloatBox& box){
                    ImageViewRGB32 image = extract_box_reference(screen, box);
                    size_t red = 0;
                    for (size_t py = 0; py < image.height(); py++)
                        for (size_t px = 0; px < image.width(); px++) {
                            uint32_t p = image.pixel(px, py);
                            int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                            red += r > g + 30 && r > b + 30;
                        }
                    return red;
                };
                size_t off = red_count({0.63, y - 0.022, 0.055, 0.044});
                size_t on = red_count({0.34, y - 0.022, 0.055, 0.044});
                // Require a clear Off highlight and reject any competing On highlight.
                return off >= 10 && on * 10 < off;
            }

            static void verify_tutorial_settings_once(VideoStream& stream, ProControllerContext& context) {
                context.wait_for_all_requests();
                MenuDetector menu;
                VideoOverlaySet detector_overlays(stream.overlay());
                menu.make_overlays(detector_overlays);
                detector_overlays.add(COLOR_CYAN, {0.04, 0.065, 0.93, 0.70}, "");
                detector_overlays.add(COLOR_BLUE, {0.01, 0.005, 0.20, 0.025}, "");
                detector_overlays.add(COLOR_RED, {0.08, 0.08, 0.82, 0.67}, "");
                auto initial = stream.video().snapshot();
                if (!initial || !menu.detect(initial)) {
                    pbf_press_button(context, BUTTON_X, 80ms, 1000ms);
                    context.wait_for_all_requests();
                }
                MenuWatcher menu_open;
                if (wait_until(stream, context, 5s, {{menu_open}}) != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Settings checkpoint: Main menu not detected.", stream);
                }
                stream.log("[Tutorial settings] Main menu detected; settling for 500ms before Options navigation.");
                pbf_wait(context, 500ms);
                context.wait_for_all_requests();
                TutorialOptionsDetector options_screen;
                TutorialOptionPositionDetector position;
                // Only two icons are available at this initial checkpoint.
                // Verify the main menu before navigating to the neighboring icon.
                bool options_open = false;
                for (size_t attempt = 0; attempt < 2; attempt++) {
                    pbf_press_dpad(context, attempt == 0 ? DPAD_RIGHT : DPAD_LEFT, 80ms, 350ms);
                    pbf_press_button(context, BUTTON_A, 80ms, 0ms);
                    if (wait_for_options_screen(stream, context, options_screen, 5s)) {
                        options_open = true;
                        break;
                    }
                    pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
                    context.wait_for_all_requests();
                    MenuWatcher back_to_menu;
                    if (wait_until(stream, context, 5s, {{back_to_menu}}) != 0) break;
                }
                if (!options_open) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Settings checkpoint: Options screen not detected.", stream);
                }
                stream.log("[Tutorial settings] Options screen detected.");
                pbf_wait(context, 500ms);
                context.wait_for_all_requests();
                struct Setting { const char* label; const char* value; };
                const Setting settings[] = {
                    {"textspeed", "fast"}, {"changewindow", "type1"},
                    {"battleeffects", "off"}, {"battlestyle", "set"},
                    {"sendtoboxes", "automatic"}, {"givenicknames", "dontgive"},
                    {"gyroscope", "use"}, {"autosave", "off"},
                    {"shownicknames", "show"}, {"backgroundmusic", "8"},
                    {"soundeffects", "8"},
                };
                for (size_t row = 0; row < sizeof(settings) / sizeof(settings[0]); row++) {
                    // Detect the selected row afresh after every navigation/correction.
                    double y = -1;
                    const std::string base = RESOURCE_PATH() + "PokemonBDSP/Autostory/Options/" + settings[row].label;
                    const ImageRGB32 label_reference(base + "_label.png");
                    const ImageRGB32 value_reference(base + "_value.png");
                    bool slider_reset = false;
                    bool correct = false;
                    for (size_t attempt = 0; attempt < 30; attempt++) {
                        context.wait_for_all_requests();
                        auto frame = stream.video().snapshot();
                        if (frame && options_screen.detect(frame)) y = position.selected_row(frame);
                        else y = -1;
                        const bool value_visible = y >= 0 && position.selected_value(frame, y);
                        const double label_score = y >= 0 ? option_image_score(frame,
                            {0.08, y - 0.025, 0.24, 0.05}, label_reference, false) : 0;
                        stream.log("[Tutorial settings] Expected row=" + std::string(settings[row].label) +
                            " row_y=" + std::to_string(y) + " highlighted_value=" + std::to_string(value_visible) +
                            " label_match=" + std::to_string(label_score));
                        if (y < 0 || !value_visible || label_score < 0.80) {
                            // A menu can be detected while its row/text is still
                            // fading in. Re-read without directional inputs first.
                            if (attempt < 4) {
                                pbf_wait(context, 200ms);
                                context.wait_for_all_requests();
                                continue;
                            }
                            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                "Settings checkpoint: Unable to confirm expected row " + std::string(settings[row].label), stream);
                        }
                        // These are regular video-overlay boxes, so the existing
                        // overlay display control can show/hide every active ROI.
                        OverlayBoxScope row_overlay(stream.overlay(), COLOR_RED,
                            {0.07, y - 0.04, 0.84, 0.08}, "");
                        OverlayBoxScope label_overlay(stream.overlay(), COLOR_YELLOW,
                            {0.08, y - 0.025, 0.24, 0.05}, "");
                        OverlayBoxScope value_overlay(stream.overlay(), COLOR_GREEN,
                            {0.33, y - 0.025, 0.54, 0.05}, "");
                        VideoOverlaySet autosave_rois(stream.overlay());
                        if (row == 7) {
                            autosave_rois.add(COLOR_GREEN, {0.63, y - 0.022, 0.055, 0.044});
                            autosave_rois.add(COLOR_RED, {0.34, y - 0.022, 0.055, 0.044});
                        }
                        double score;
                        if (row >= 9) {
                            // Compare the numeric value rather than the long slider:
                            // neighboring slider levels otherwise look too similar.
                            ImageViewRGB32 number = value_reference.sub_image(
                                value_reference.width() * 82 / 100, 0,
                                value_reference.width() - value_reference.width() * 82 / 100,
                                value_reference.height());
                            ImageRGB32 number_reference = number.scale_to(96, 32);
                            score = option_image_score(frame,
                                {0.7728, y - 0.025, 0.0972, 0.05}, number_reference, true);
                        } else {
                            score = option_image_score(frame,
                                {0.33, y - 0.025, 0.54, 0.05}, value_reference, true);
                        }
                        stream.log("[Tutorial settings] " + std::string(settings[row].label) +
                            " target match=" + std::to_string(score));
                        // The reference recording uses inactive pale-red text;
                        // the focused live row has a thicker dark-red glyph mask.
                        // Verified Fast scores 0.734 while Slow/Normal score 0.
                        if (score >= 0.65 && (row != 7 || autosave_off_highlight(frame, y))) {
                            if (row == 7) {
                                for (size_t confirmation = 0; confirmation < 3; confirmation++) {
                                    pbf_wait(context, 200ms);
                                    context.wait_for_all_requests();
                                    auto check = stream.video().snapshot();
                                    double check_y = check ? position.selected_row(check) : -1;
                                    if (!check || !options_screen.detect(check) || check_y < 0 ||
                                        option_image_score(check, {0.08, check_y - 0.025, 0.24, 0.05}, label_reference, false) < 0.80 ||
                                        !autosave_off_highlight(check, check_y)) {
                                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                            "Autosave Off could not be confirmed. Stopping before movement.", stream);
                                    }
                                }
                            }
                            stream.log("[Tutorial settings] Verified " + std::string(settings[row].label) + "=" + settings[row].value);
                            correct = true;
                            break;
                        }
                        stream.log("[Tutorial settings] Correcting " + std::string(settings[row].label));
                        if (row >= 9 && !slider_reset) {
                            // Sliders clamp at zero. Establish a known endpoint,
                            // then increment; verify the target image afterward.
                            for (size_t step = 0; step < 12; step++)
                                pbf_press_dpad(context, DPAD_LEFT, 80ms, 100ms);
                            slider_reset = true;
                        } else {
                            pbf_press_dpad(context, DPAD_RIGHT, 80ms, 350ms);
                        }
                        context.wait_for_all_requests();
                    }
                    if (!correct) {
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "Settings checkpoint: Could not apply " + std::string(settings[row].label), stream);
                    }
                    if (row + 1 < sizeof(settings) / sizeof(settings[0]))
                        pbf_press_dpad(context, DPAD_DOWN, 80ms, 350ms);
                }
                // Confirm settings before exiting: A A A A B B.
                pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
                pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
                pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
                pbf_press_button(context, BUTTON_A, 80ms, 1200ms);
                pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
                pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
                context.wait_for_all_requests();
                OverworldWatcher overworld;
                if (wait_until(stream, context, 10s, {{overworld}}) != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Settings checkpoint: Did not return to the overworld after saving options.", stream);
                }
                // Reopen the saved Options screen and independently verify Autosave.
                // No route movement is permitted until this persisted-value check passes.
                pbf_press_button(context, BUTTON_X, 80ms, 1000ms);
                context.wait_for_all_requests();
                MenuWatcher verify_menu;
                if (wait_until(stream, context, 5s, {{verify_menu}}) != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Autosave verification: Main menu not detected.", stream);
                }
                bool reopened = false;
                for (size_t attempt = 0; attempt < 2; attempt++) {
                    pbf_press_button(context, BUTTON_A, 80ms, 0ms);
                    if (wait_for_options_screen(stream, context, options_screen, 5s)) { reopened = true; break; }
                    pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
                    context.wait_for_all_requests();
                    MenuWatcher return_menu;
                    if (wait_until(stream, context, 5s, {{return_menu}}) != 0) break;
                    pbf_press_dpad(context, DPAD_RIGHT, 80ms, 350ms);
                }
                if (!reopened) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Autosave verification: Could not reopen saved Options.", stream);
                }
                // Options starts on Text Speed; Autosave is seven rows below it.
                for (size_t step = 0; step < 7; step++)
                    pbf_press_dpad(context, DPAD_DOWN, 80ms, 350ms);
                context.wait_for_all_requests();
                const ImageRGB32 autosave_label(RESOURCE_PATH() + "PokemonBDSP/Autostory/Options/autosave_label.png");
                for (size_t confirmation = 0; confirmation < 3; confirmation++) {
                    auto check = stream.video().snapshot();
                    double check_y = check ? position.selected_row(check) : -1;
                    if (!check || !options_screen.detect(check) || check_y < 0 ||
                        option_image_score(check, {0.08, check_y - 0.025, 0.24, 0.05}, autosave_label, false) < 0.80 ||
                        !autosave_off_highlight(check, check_y)) {
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "Saved Autosave setting is not verified Off. Stopping before movement.", stream);
                    }
                    pbf_wait(context, 200ms);
                    context.wait_for_all_requests();
                }
                stream.log("[Tutorial settings] Autosave = Off: confirmed after reopening saved Options (3 frames).");
                // Confirm settings before exiting: A A A A B B.
                pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
                pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
                pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
                pbf_press_button(context, BUTTON_A, 80ms, 1200ms);
                pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
                pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
                context.wait_for_all_requests();
                OverworldWatcher verified_exit;
                if (wait_until(stream, context, 10s, {{verified_exit}}) != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Autosave verification: Could not return to the overworld.", stream);
                }
                // Report inferred values explicitly: these are visual matches,
                // allowing the user to compare our conclusions with the screen.
                stream.log("[Tutorial settings] Completed. Values believed applied (visual matches before saving):");
                stream.log("[Tutorial settings] Text Speed = Fast; Change Window = Type 1; Battle Effects = Off; Battle Style = Set.");
                stream.log("[Tutorial settings] Send to Boxes = Automatic; Give Nicknames = Don't give; Gyroscope = Use.");
                stream.log("[Tutorial settings] Autosave = Off; Show Nicknames = Show; Background Music = 8; Sound Effects = 8.");
            }


            void verify_tutorial_settings(VideoStream& stream, ProControllerContext& context) {
                // Settings-only recovery: initial attempt plus three retries.
                // Never permit route movement unless the complete check succeeds.
                constexpr size_t MAX_RETRIES = 3;
                for (size_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
                    stream.log("[Tutorial settings] Attempt " + std::to_string(attempt + 1) + "/4.");
                    try {
                        verify_tutorial_settings_once(stream, context);
                        return;
                    } catch (OperationFailedException& error) {
                        stream.log("[Tutorial settings] Attempt failed: " + error.message(), COLOR_ORANGE);
                        if (attempt == MAX_RETRIES) {
                            stream.log("[Tutorial settings] All three retries exhausted; stopping before movement.", COLOR_RED);
                            throw;
                        }
                        // Back out of Options, save prompts, bag, or menu without
                        // moving the player or resetting/loading the game.
                        stream.log("[Tutorial settings] Returning to overworld before retry.");
                        context.wait_for_all_requests();
                        TutorialOptionsDetector options_screen;
                        const auto failed_frame = stream.video().snapshot();
                        if (failed_frame && options_screen.detect(failed_frame)) {
                            // Finish the known Options save/exit sequence before
                            // observing state; never cancel it on an Options frame
                            // misclassified as overworld by the generic detector.
                            for (size_t press = 0; press < 4; press++)
                                pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
                        }
                        pbf_mash_button(context, BUTTON_B, 3000ms);
                        context.wait_for_all_requests();
                        OverworldDetector overworld;
                        MenuDetector menu;
                        BlackScreenDetector black;
                        bool recovered = false;
                        size_t confirmations = 0;
                        for (size_t check = 0; check < 15; check++) {
                            const auto frame = stream.video().snapshot();
                            const bool clear = frame && !options_screen.detect(frame) &&
                                !menu.detect(frame) && !black.detect(frame) && bag_pocket_position(frame) == 0 && overworld.detect(frame);
                            if (clear) {
                                if (++confirmations >= 3) { recovered = true; break; }
                                pbf_wait(context, 200ms);
                            } else {
                                confirmations = 0;
                                pbf_mash_button(context, BUTTON_B, 500ms);
                            }
                            context.wait_for_all_requests();
                        }
                        stream.log(recovered ? "[Tutorial settings] Recovery confirmed outside Options/menu; retrying."
                            : "[Tutorial settings] Recovery not confirmed; next attempt will recheck the screen.");
                        pbf_wait(context, 500ms);
                        context.wait_for_all_requests();
                    }
                }
            }

            void set_time_fast(
                SingleSwitchProgramEnvironment& env,
                ProControllerContext& context,
                const DateTime& target,
                bool preserve_date
            ) {
                env.log("[AutoStory] Fast date set starting...", COLOR_ORANGE);

                // 1) Home
                NintendoSwitch::go_home(env.console, context);

                // 2) Navigate to Date/Time settings quickly.
                NintendoSwitch::home_to_date_time(env.console, context, true);

                // 3) Enter date editor.
                pbf_press_button(context, BUTTON_A, 80ms, 240ms);

                // 4) Wait for date screen and set exact time.
                NintendoSwitch::DateChangeWatcher date_reader(env.console);
                int ret = wait_until(
                    env.console, context, 10s,
                    { date_reader }
                );
                if (ret < 0) {
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "Failed to enter Date Change screen.",
                        env.console
                    );
                }

                {
                    VideoOverlaySet overlays(env.console.overlay());
                    date_reader.make_overlays(overlays);
                    DateTime effective_target = target;
                    if (preserve_date) {
                        context.wait_for_all_requests();
                        const auto snapshot = env.console.video().snapshot();
                        if (!snapshot) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "Clock-only setup: No date-editor frame.", env.console);
                        const DateTime current = date_reader.read_date(env.console, snapshot.frame).second;
                        if (current.year < 2000 || current.year > 2060 || current.month < 1 || current.month > 12 ||
                            current.day < 1 || current.day > 31)
                            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                "Clock-only setup: Could not reliably read the current console date.", env.console);
                        effective_target.year = current.year;
                        effective_target.month = current.month;
                        effective_target.day = current.day;
                        env.log("[AutoStory] Clock-only target=" + std::to_string((int)target.hour) + ":" +
                            std::to_string((int)target.minute) + "; preserving date=" + std::to_string(current.year) + "-" +
                            std::to_string((int)current.month) + "-" + std::to_string((int)current.day) + ".");
                    }
                    date_reader.set_date(env.program_info(), env.console, context, effective_target);
                }

                // 5) Commit date.
                pbf_press_button(context, BUTTON_A, 160ms, 240ms);

                // 6) Back to game.
                pbf_press_button(
                    context, BUTTON_HOME, 160ms,
                    ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0
                );
                NintendoSwitch::resume_game_from_home(env.console, context, false);

                context.wait_for_all_requests();
                context.wait_for(500ms);
            }

        }
    }
}
