/*  BDSP AutoStory - Settings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include <algorithm>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP_AutoStory_Bag.h"
#include "PokemonBDSP_AutoStory_Settings.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

// Options chrome and position detectors are independent of setting values.
class TutorialOptionsDetector {
public:
    bool detect(const ImageViewRGB32& screen) const {
        ImageViewRGB32 panel = extract_box_reference(screen, ImageFloatBox{0.04, 0.065, 0.93, 0.70});
        size_t white = 0;
        for (size_t y = 0; y < panel.height(); y += 4)
            for (size_t x = 0; x < panel.width(); x += 4){
                uint32_t p = panel.pixel(x, y);
                if (((p >> 16) & 255) > 225 && ((p >> 8) & 255) > 225 && (p & 255) > 225){
                    white++;
                }
            }
        double samples = (double)(((panel.width() + 3) / 4) * ((panel.height() + 3) / 4));
        // Distinguish Options from the icon menu and bag using its
        // blue heading and full-width white settings panel.
        ImageViewRGB32 heading = extract_box_reference(screen, ImageFloatBox{0.01, 0.005, 0.20, 0.025});
        size_t blue = 0, total = 0;
        for (size_t y = 0; y < heading.height(); y++)
            for (size_t x = 0; x < heading.width(); x++){
                uint32_t p = heading.pixel(x, y);
                int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                blue += b > r + 20 && g > r + 10;
                total++;
            }
        return samples > 0 && white / samples > 0.70 && total > 0 && blue > total / 4;
    }
};

// Options fades in after A on its icon: poll instead of judging one frame.
bool wait_for_options_screen(VideoStream& stream, ProControllerContext& context,
    const TutorialOptionsDetector& options_screen, Milliseconds timeout){
    context.wait_for_all_requests();
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        auto frame = stream.video().snapshot();
        if (frame && options_screen.detect(frame)){
            return true;
        }
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
        for (size_t y = (size_t)(screen.height() * 0.08); y < screen.height() * 0.75; y++){
            size_t red = 0, total = 0;
            for (size_t x = (size_t)(screen.width() * 0.08); x < screen.width() * 0.90; x += 2){
                uint32_t p = screen.pixel(x, y);
                int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                red += r > 150 && r > g + 40 && r > b + 40;
                total++;
            }
            if (total && red > total * 0.70){
                if (first < 0){
                    first = (int)y;
                }
                last = (int)y;
            }
        }
        if (first < 0 || last - first < screen.height() * 0.04 || last - first > screen.height() * 0.12){
            return -1;
        }
        return (first + last) / (2.0 * screen.height());
    }
    // Locate selected red value text separately from the row border.
    bool selected_value(const ImageViewRGB32& screen, double y) const {
        ImageViewRGB32 value = extract_box_reference(screen, ImageFloatBox{0.33, y - 0.025, 0.54, 0.05});
        size_t red = 0;
        for (size_t py = 0; py < value.height(); py++)
            for (size_t px = 0; px < value.width(); px++){
                uint32_t p = value.pixel(px, py);
                int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                red += r > g + 20 && r > b + 20;
            }
        return red > value.width() * value.height() * 0.0005;
    }
};


// Compare binary visual shapes with small alignment tolerance.
// No text recognition is needed for fixed English option labels.
double option_image_score(const ImageViewRGB32& screen,
    const ImageFloatBox& box, const ImageRGB32& reference, bool red_only){
    ImageRGB32 actual = extract_box_reference(screen, box).scale_to(384, 32);
    ImageRGB32 expected = reference.scale_to(384, 32);
    auto ink = [red_only](uint32_t p){
        int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
        return red_only ? r > g + 20 && r > b + 20 : r + g + b < 500;
    };
    double best = 0;
    for (int dy = -2; dy <= 2; dy++){
        for (int dx = -2; dx <= 2; dx++){
            size_t overlap = 0, count = 0;
            for (int y = 2; y < 30; y++){
                for (int x = 2; x < 382; x++){
                    bool a = ink(actual.pixel(x + dx, y + dy));
                    bool b = ink(expected.pixel(x, y));
                    overlap += a && b;
                    count += a + b;
                }
            }
            if (count){
                best = std::max(best, 2.0 * overlap / count);
            }
        }
    }
    return best;
}

bool autosave_off_highlight(const ImageViewRGB32& screen, double y){
    auto red_count = [&](const ImageFloatBox& box){
        ImageViewRGB32 image = extract_box_reference(screen, box);
        size_t red = 0;
        for (size_t py = 0; py < image.height(); py++)
            for (size_t px = 0; px < image.width(); px++){
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

//  The options the story needs, as each reference image names them: the
//  row's label and the value to select.
struct Setting{
    const char* label;
    const char* value;
};
const Setting SETTINGS[] = {
    {"textspeed", "fast"}, {"changewindow", "type1"},
    {"battleeffects", "off"}, {"battlestyle", "set"},
    {"sendtoboxes", "automatic"}, {"givenicknames", "dontgive"},
    {"gyroscope", "use"}, {"autosave", "off"},
    {"shownicknames", "show"}, {"backgroundmusic", "8"},
    {"soundeffects", "8"},
};
constexpr size_t SETTING_COUNT = sizeof(SETTINGS) / sizeof(SETTINGS[0]);
constexpr size_t AUTOSAVE_ROW = 7;
constexpr size_t FIRST_SLIDER_ROW = 9;

//  The Options screen and its selected row, as one reader.
struct OptionsScreen{
    TutorialOptionsDetector options_screen;
    TutorialOptionPositionDetector position;
};
//  A row's label and target value, as the reference images show them.
struct OptionReferences{
    ImageRGB32 label;
    ImageRGB32 value;
};

//  From the overworld (or the main menu): the main menu, settled.
void open_main_menu(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    MenuDetector menu(COLOR_RED, /*skip_new_banner*/ true);
    VideoOverlaySet detector_overlays(stream.overlay());
    menu.make_overlays(detector_overlays);
    detector_overlays.add(COLOR_CYAN, {0.04, 0.065, 0.93, 0.70}, "");
    detector_overlays.add(COLOR_BLUE, {0.01, 0.005, 0.20, 0.025}, "");
    detector_overlays.add(COLOR_RED, {0.08, 0.08, 0.82, 0.67}, "");
    auto initial = stream.video().snapshot();
    if (!initial || !menu.detect(initial)){
        pbf_press_button(context, BUTTON_X, 80ms, 1000ms);
        context.wait_for_all_requests();
    }
    MenuWatcher menu_open(COLOR_RED, /*skip_new_banner*/ true);
    if (wait_until(stream, context, 5s, {{menu_open}}) != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Settings checkpoint: Main menu not detected.", stream);
    }
    stream.log("[Tutorial settings] Main menu detected; settling for 500ms before Options navigation.");
    pbf_wait(context, 500ms);
    context.wait_for_all_requests();
}

//  From the main menu: Options. Only two icons are available at this
//  initial checkpoint; verify the main menu before trying the other.
void open_options(VideoStream& stream, ProControllerContext& context, const TutorialOptionsDetector& options_screen){
    bool options_open = false;
    for (size_t attempt = 0; attempt < 2; attempt++){
        pbf_press_dpad(context, attempt == 0 ? DPAD_RIGHT : DPAD_LEFT, 80ms, 350ms);
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        if (wait_for_options_screen(stream, context, options_screen, 5s)){
            options_open = true;
            break;
        }
        pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
        context.wait_for_all_requests();
        MenuWatcher back_to_menu(COLOR_RED, /*skip_new_banner*/ true);
        if (wait_until(stream, context, 5s, {{back_to_menu}}) != 0){
            break;
        }
    }
    if (!options_open){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Settings checkpoint: Options screen not detected.", stream);
    }
    stream.log("[Tutorial settings] Options screen detected.");
    pbf_wait(context, 500ms);
    context.wait_for_all_requests();
}

//  How well the selected row's value matches the reference.
double option_value_score(const ImageViewRGB32& frame, double y, size_t row, const ImageRGB32& value_reference){
    if (row >= FIRST_SLIDER_ROW){
        // Compare the numeric value rather than the long slider:
        // neighboring slider levels otherwise look too similar.
        ImageViewRGB32 number = value_reference.sub_image(
            value_reference.width() * 82 / 100, 0,
            value_reference.width() - value_reference.width() * 82 / 100,
            value_reference.height());
        ImageRGB32 number_reference = number.scale_to(96, 32);
        return option_image_score(frame,
            {0.7728, y - 0.025, 0.0972, 0.05}, number_reference, true);
    }
    return option_image_score(frame,
        {0.33, y - 0.025, 0.54, 0.05}, value_reference, true);
}

//  Whether Options shows Autosave selected and set to Off: a frame's look.
bool autosave_off_on_screen(
    const ImageViewRGB32& check, const TutorialOptionsDetector& options_screen,
    const TutorialOptionPositionDetector& position, const ImageRGB32& autosave_label
){
    const double check_y = check ? position.selected_row(check) : -1;
    return check && options_screen.detect(check) && check_y >= 0 &&
        option_image_score(check, {0.08, check_y - 0.025, 0.24, 0.05}, autosave_label, false) >= 0.80 &&
        autosave_off_highlight(check, check_y);
}

//  Autosave Off, on three more frames 200 ms apart.
void confirm_autosave_off(
    VideoStream& stream, ProControllerContext& context, const TutorialOptionsDetector& options_screen,
    const TutorialOptionPositionDetector& position, const ImageRGB32& label_reference
){
    for (size_t confirmation = 0; confirmation < 3; confirmation++){
        pbf_wait(context, 200ms);
        context.wait_for_all_requests();
        auto check = stream.video().snapshot();
        if (!autosave_off_on_screen(check, options_screen, position, label_reference)){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Autosave Off could not be confirmed. Stopping before movement.", stream);
        }
    }
}

//  One press toward the target value. Sliders clamp at zero: establish a
//  known endpoint, then increment; the target image is verified afterward.
void correct_option(ProControllerContext& context, size_t row, bool& slider_reset){
    if (row >= FIRST_SLIDER_ROW && !slider_reset){
        for (size_t step = 0; step < 12; step++){
            pbf_press_dpad(context, DPAD_LEFT, 80ms, 100ms);
        }
        slider_reset = true;
    }else{
        pbf_press_dpad(context, DPAD_RIGHT, 80ms, 350ms);
    }
    context.wait_for_all_requests();
}

//  The selected row on screen, if it is the expected one with its value
//  highlighted; -1 otherwise.
double expected_row_y(
    VideoStream& stream, const ImageViewRGB32& frame, const TutorialOptionsDetector& options_screen,
    const TutorialOptionPositionDetector& position, size_t row, const ImageRGB32& label_reference
){
    const double y = frame && options_screen.detect(frame) ? position.selected_row(frame) : -1;
    const bool value_visible = y >= 0 && position.selected_value(frame, y);
    const double label_score = y >= 0 ? option_image_score(frame,
        {0.08, y - 0.025, 0.24, 0.05}, label_reference, false) : 0;
    stream.log("[Tutorial settings] Expected row=" + std::string(SETTINGS[row].label) +
        " row_y=" + std::to_string(y) + " highlighted_value=" + std::to_string(value_visible) +
        " label_match=" + std::to_string(label_score));
    return y >= 0 && value_visible && label_score >= 0.80 ? y : -1;
}

//  Whether the selected row (at `y`) shows the target value: checked on
//  the frame, with the row's boxes on the overlay.
bool option_verified(
    VideoStream& stream, ProControllerContext& context, const ImageViewRGB32& frame, double y, size_t row,
    const OptionsScreen& screen, const OptionReferences& references
){
    const ImageRGB32& value_reference = references.value;
    // These are regular video-overlay boxes, so the existing overlay
    // display control can show/hide every active ROI.
    OverlayBoxScope row_overlay(stream.overlay(), COLOR_RED, {0.07, y - 0.04, 0.84, 0.08}, "");
    OverlayBoxScope label_overlay(stream.overlay(), COLOR_YELLOW, {0.08, y - 0.025, 0.24, 0.05}, "");
    OverlayBoxScope value_overlay(stream.overlay(), COLOR_GREEN, {0.33, y - 0.025, 0.54, 0.05}, "");
    VideoOverlaySet autosave_rois(stream.overlay());
    if (row == AUTOSAVE_ROW){
        autosave_rois.add(COLOR_GREEN, {0.63, y - 0.022, 0.055, 0.044});
        autosave_rois.add(COLOR_RED, {0.34, y - 0.022, 0.055, 0.044});
    }
    const double score = option_value_score(frame, y, row, value_reference);
    stream.log("[Tutorial settings] " + std::string(SETTINGS[row].label) + " target match=" + std::to_string(score));
    // The reference recording uses inactive pale-red text; the focused live
    // row has a thicker dark-red glyph mask. Verified Fast scores 0.734
    // while Slow/Normal score 0.
    if (score < 0.65 || (row == AUTOSAVE_ROW && !autosave_off_highlight(frame, y))){
        return false;
    }
    if (row == AUTOSAVE_ROW){
        confirm_autosave_off(stream, context, screen.options_screen, screen.position, references.label);
    }
    return true;
}

//  Sets the option on `row`, the row selected now, to its target value.
void set_option(VideoStream& stream, ProControllerContext& context, size_t row, const OptionsScreen& screen){
    const std::string base = RESOURCE_PATH() + "PokemonBDSP/Autostory/Options/" + SETTINGS[row].label;
    const OptionReferences references{ImageRGB32(base + "_label.png"), ImageRGB32(base + "_value.png")};
    bool slider_reset = false;
    for (size_t attempt = 0; attempt < 30; attempt++){
        // Detect the selected row afresh after every navigation/correction.
        context.wait_for_all_requests();
        auto frame = stream.video().snapshot();
        const double y = expected_row_y(stream, frame, screen.options_screen, screen.position, row, references.label);
        if (y < 0){
            // A menu can be detected while its row/text is still fading
            // in. Re-read without directional inputs first.
            if (attempt < 4){
                pbf_wait(context, 200ms);
                context.wait_for_all_requests();
                continue;
            }
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Settings checkpoint: Unable to confirm expected row " + std::string(SETTINGS[row].label), stream);
        }
        if (option_verified(stream, context, frame, y, row, screen, references)){
            stream.log("[Tutorial settings] Verified " + std::string(SETTINGS[row].label) + "=" + SETTINGS[row].value);
            return;
        }
        stream.log("[Tutorial settings] Correcting " + std::string(SETTINGS[row].label));
        correct_option(context, row, slider_reset);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        "Settings checkpoint: Could not apply " + std::string(SETTINGS[row].label), stream);
}

//  Confirms the settings and leaves Options (A A A A B B), back to the overworld.
void save_options(VideoStream& stream, ProControllerContext& context, const std::string& failure){
    pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
    pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
    pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
    pbf_press_button(context, BUTTON_A, 80ms, 1200ms);
    pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
    pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
    context.wait_for_all_requests();
    OverworldWatcher overworld;
    if (wait_until(stream, context, 10s, {{overworld}}) != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, failure, stream);
    }
}

//  From the overworld: the saved Options screen again.
void reopen_options(VideoStream& stream, ProControllerContext& context, const TutorialOptionsDetector& options_screen){
    pbf_press_button(context, BUTTON_X, 80ms, 1000ms);
    context.wait_for_all_requests();
    MenuWatcher verify_menu(COLOR_RED, /*skip_new_banner*/ true);
    if (wait_until(stream, context, 5s, {{verify_menu}}) != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Autosave verification: Main menu not detected.", stream);
    }
    bool reopened = false;
    for (size_t attempt = 0; attempt < 2; attempt++){
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        if (wait_for_options_screen(stream, context, options_screen, 5s)){
            reopened = true;
            break;
        }
        pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
        context.wait_for_all_requests();
        MenuWatcher return_menu(COLOR_RED, /*skip_new_banner*/ true);
        if (wait_until(stream, context, 5s, {{return_menu}}) != 0){
            break;
        }
        pbf_press_dpad(context, DPAD_RIGHT, 80ms, 350ms);
    }
    if (!reopened){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Autosave verification: Could not reopen saved Options.", stream);
    }
}

//  In the saved Options: Autosave is Off on three frames. Options starts
//  on Text Speed; Autosave is seven rows below it.
void check_saved_autosave(
    VideoStream& stream, ProControllerContext& context,
    const TutorialOptionsDetector& options_screen, const TutorialOptionPositionDetector& position
){
    for (size_t step = 0; step < AUTOSAVE_ROW; step++){
        pbf_press_dpad(context, DPAD_DOWN, 80ms, 350ms);
    }
    context.wait_for_all_requests();
    const ImageRGB32 autosave_label(RESOURCE_PATH() + "PokemonBDSP/Autostory/Options/autosave_label.png");
    for (size_t confirmation = 0; confirmation < 3; confirmation++){
        auto check = stream.video().snapshot();
        if (!autosave_off_on_screen(check, options_screen, position, autosave_label)){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Saved Autosave setting is not verified Off. Stopping before movement.", stream);
        }
        pbf_wait(context, 200ms);
        context.wait_for_all_requests();
    }
    stream.log("[Tutorial settings] Autosave = Off: confirmed after reopening saved Options (3 frames).");
}

void verify_tutorial_settings_once(VideoStream& stream, ProControllerContext& context){
    open_main_menu(stream, context);
    OptionsScreen screen;
    open_options(stream, context, screen.options_screen);
    for (size_t row = 0; row < SETTING_COUNT; row++){
        set_option(stream, context, row, screen);
        if (row + 1 < SETTING_COUNT){
            pbf_press_dpad(context, DPAD_DOWN, 80ms, 350ms);
        }
    }
    save_options(stream, context, "Settings checkpoint: Did not return to the overworld after saving options.");
    // Reopen the saved Options screen and independently verify Autosave.
    // No route movement is permitted until this persisted-value check passes.
    reopen_options(stream, context, screen.options_screen);
    check_saved_autosave(stream, context, screen.options_screen, screen.position);
    save_options(stream, context, "Autosave verification: Could not return to the overworld.");
    // Report inferred values explicitly: these are visual matches,
    // allowing the user to compare our conclusions with the screen.
    stream.log("[Tutorial settings] Completed. Values believed applied (visual matches before saving):");
    stream.log("[Tutorial settings] Text Speed = Fast; Change Window = Type 1; Battle Effects = Off; Battle Style = Set.");
    stream.log("[Tutorial settings] Send to Boxes = Automatic; Give Nicknames = Don't give; Gyroscope = Use.");
    stream.log("[Tutorial settings] Autosave = Off; Show Nicknames = Show; Background Music = 8; Sound Effects = 8.");
}

//  After a failed attempt: back out of Options, save prompts, the Bag or
//  the menu without moving the player or resetting/loading the game.
void back_to_overworld(VideoStream& stream, ProControllerContext& context){
    stream.log("[Tutorial settings] Returning to overworld before retry.");
    context.wait_for_all_requests();
    TutorialOptionsDetector options_screen;
    const auto failed_frame = stream.video().snapshot();
    if (failed_frame && options_screen.detect(failed_frame)){
        // Finish the known Options save/exit sequence before observing
        // state; never cancel it on an Options frame misclassified as
        // overworld by the generic detector.
        for (size_t press = 0; press < 4; press++){
            pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
        }
    }
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
    OverworldDetector overworld;
    MenuDetector menu(COLOR_RED, /*skip_new_banner*/ true);
    BlackScreenDetector black;
    bool recovered = false;
    size_t confirmations = 0;
    for (size_t check = 0; check < 15 && !recovered; check++){
        const auto frame = stream.video().snapshot();
        const bool clear = frame && !options_screen.detect(frame) &&
            !menu.detect(frame) && !black.detect(frame) && bag_pocket_position(frame) == 0 && overworld.detect(frame);
        if (clear && ++confirmations >= 3){
            recovered = true;
        }else if (clear){
            pbf_wait(context, 200ms);
        }else{
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

void verify_tutorial_settings(VideoStream& stream, ProControllerContext& context){
    // Settings-only recovery: initial attempt plus three retries.
    // Never permit route movement unless the complete check succeeds.
    constexpr size_t MAX_RETRIES = 3;
    for (size_t attempt = 0; attempt <= MAX_RETRIES; attempt++){
        stream.log("[Tutorial settings] Attempt " + std::to_string(attempt + 1) + "/4.");
        try{
            verify_tutorial_settings_once(stream, context);
            return;
        }catch (OperationFailedException& error){
            stream.log("[Tutorial settings] Attempt failed: " + error.message(), COLOR_ORANGE);
            if (attempt == MAX_RETRIES){
                stream.log("[Tutorial settings] All three retries exhausted; stopping before movement.", COLOR_RED);
                throw;
            }
            back_to_overworld(stream, context);
        }
    }
}



}
}
}
