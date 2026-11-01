/*  BDSP AutoStory - Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include <cmath>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameNavigation.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_Actions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  Opens the save prompt from the overworld and keeps a screenshot of it: it
//  shows the play time and badges, a record of where each checkpoint save
//  stands. Returns the map its card names.
std::string open_save_prompt(VideoStream& stream, ProControllerContext& context){
    overworld_to_menu(stream, context);
    pbf_press_button(context, BUTTON_R, 80ms, 2000ms);
    context.wait_for_all_requests();
    VideoSnapshot screen = stream.video().snapshot();
    if (!screen){
        return "";
    }
    const std::string folder = SCREENSHOTS_PATH() + "AutoStory/saves/";
    Filesystem::create_directories(Filesystem::Path(folder));
    const std::string path = folder + now_to_filestring() + ".png";
    screen.frame->save(path);
    stream.log("Save prompt screenshot: " + path);
    const std::string location = read_save_screen_location(*screen.frame);
    stream.log("Save screen location: \"" + location + "\".");
    return location;
}

//  Backs out of the save prompt and its menu to the overworld.
void leave_save_prompt(VideoStream& stream, ProControllerContext& context){
    pbf_mash_button(context, BUTTON_B, 2000ms);
    context.wait_for_all_requests();
    OverworldWatcher overworld;
    if (wait_until(stream, context, std::chrono::seconds(15), {overworld}) < 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Failed to detect the overworld after leaving the save prompt.", stream);
    }
}

struct IconGrid {
    int row, col;
};

IconGrid icon_to_grid(MenuCursorPosition pos, int num_icons){
    switch (pos){
    case MenuCursorPosition::POKEDEX:
        return {0, 0};
    case MenuCursorPosition::POKEMON:
        return {0, 1};
    case MenuCursorPosition::BAG:
        return {0, 2};
    case MenuCursorPosition::CARD:
        return {0, 3};
    case MenuCursorPosition::OPTIONS:
        return {1, num_icons <= 5 ? 0 : 2};
    case MenuCursorPosition::MAP:
        return {1, 0};
    case MenuCursorPosition::CAPSULES:
        return {1, 1};
    case MenuCursorPosition::MYSTERYGIFT:
        return {1, 3};
    default:
        return {-1, -1};
    }
}

}

std::string read_location_from_save_screen(VideoStream& stream, ProControllerContext& context){
    const std::string location = open_save_prompt(stream, context);
    leave_save_prompt(stream, context);
    return location;
}

bool save_game(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const std::string& expected_location
) {
    static constexpr size_t MAX_RETRIES = 3;

    std::string wrong_place;
    for (size_t attempt = 0; attempt < MAX_RETRIES && wrong_place.empty(); attempt++){
        try {
            env.console.log("Checkpoint save attempt: " + std::to_string(attempt + 1));

            // Only mash B on retries - no need to on first attempt
            // if already cleanly in the overworld.
            if (attempt > 0){
                pbf_mash_button(context, BUTTON_B, 1000ms);
            }

            // The feedback-based save of save_game(VideoStream&, ...), split at the
            // save prompt to check where it stands.
            const std::string location = open_save_prompt(env.console, context);
            if (!expected_location.empty() && !location_banner_matches(location, expected_location)){
                wrong_place = "About to save in \"" + location + "\", expected " + expected_location + "; not saving.";
                env.console.log(wrong_place, COLOR_RED);
                leave_save_prompt(env.console, context);
                break;
            }
            pbf_press_button(context, BUTTON_ZL, 80ms, 5000ms);

            // Return to overworld cleanly.
            pbf_mash_button(context, BUTTON_B, 1000ms);

            // Drain R/confirm/B before detection; an earlier overworld frame
            // must not report success while saving commands are still queued.
            context.wait_for_all_requests();

            // Verify overworld. Generous timeout: save animation + fade can be slow.
            OverworldWatcher overworld;
            int ret = wait_until(env.console, context, std::chrono::seconds(15), {overworld});
            if (ret < 0){
                throw OperationFailedExceptionWithScreenshot(ErrorReportMode::SEND_ERROR_REPORT,
                    "Failed to detect overworld after save.",
                    env.console);
            }

            env.console.log("Checkpoint save verified.");
            return true;

        } catch (OperationFailedException& e){
            env.console.log("Checkpoint save failed: " + e.message(), COLOR_RED);
            context.wait_for(1000ms);
        }
    }
    //  No blind fallback for the wrong place: it would save there.
    if (!wrong_place.empty()){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, wrong_place, env.console);
    }

    // Blind fallback: reuse the non-feedback save_game(context) from GameNavigation
    // rather than duplicating the button sequence manually.
    env.console.log("All save retries failed. Using blind fallback.", COLOR_ORANGE);
    pbf_mash_button(context, BUTTON_B, 1000ms);
    save_game(context); // PokemonBDSP_GameNavigation.h - non-feedback overload
    pbf_mash_button(context, BUTTON_B, 2000ms);
    context.wait_for_all_requests();

    return false;
}

void wait_for_dialogue(VideoStream& stream,
    ProControllerContext& context,
    const std::string& label,
    Milliseconds timeout){
    auto log_detected = [&] {
        if (!label.empty()){
            stream.log(label + " dialogue detected.");
        }
    };

    ShortDialogWatcher watcher_cyan(COLOR_CYAN);
    int ret = run_until<ProControllerContext>(stream,
        context,
        [timeout](ProControllerContext& context){
            context.wait_for(timeout);
        },
        {{watcher_cyan}});
    if (ret == 0){
        log_detected();
        return;
    }

    ShortDialogWatcher watcher_red(COLOR_RED);
    ret = run_until<ProControllerContext>(stream,
        context,
        [timeout](ProControllerContext& context){
            context.wait_for(timeout);
        },
        {{watcher_red}});
    if (ret == 0){
        log_detected();
        return;
    }

    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        (label.empty() ? "wait_for_dialogue" : label) + ": dialogue not detected within timeout.",
        stream);
}

void open_menu(VideoStream& stream, ProControllerContext& context, MenuCursorPosition target, int num_icons){
    using namespace std::chrono_literals;

    // Previous single opening attempt:
    // context.wait_for_all_requests();
    // overworld_to_menu(stream, context);
    // pbf_wait(context, 1000ms);
    // context.wait_for_all_requests();
    context.wait_for_all_requests();

    MenuCursorDetector detector;
    {
        static const char* ICON_NAMES[MENU_CURSOR_NUM_ICONS] = {
            "POKEDEX",
            "POKEMON",
            "BAG",
            "CARD",
            "OPTIONS",
            "MAP",
            "CAPSULES",
            "MYSTERYGIFT",
        };
        // Previous optional template directory: PokemonBDSP/AutoStory.
        std::string ref_dir = RESOURCE_PATH() + "PokemonBDSP/Autostory";
        for (int i = 0; i < MENU_CURSOR_NUM_ICONS; i++){
            std::string path = ref_dir + "/" + ICON_NAMES[i] + ".png";
            // These templates are optional. The dark-pixel cursor detector
            // remains available when a template is not packaged.
            if (!Filesystem::is_regular_file(path)){
                continue;
            }
            // Previously constructed the image before checking its availability.
            ImageRGB32 img(path);
            if (img){
                detector.set_icon_reference(static_cast<MenuCursorPosition>(i), std::move(img));
            }
        }
    }
    // Previous single detection attempt retained for comparison.
    // MenuCursorPosition cur = MenuCursorPosition::UNKNOWN;
    // for (int i = 0; i < 10 && cur == MenuCursorPosition::UNKNOWN; i++){
    //     context.wait_for(50ms);
    //     VideoSnapshot snap = stream.video().snapshot();
    //     if (snap) cur = detector.detect(snap);
    // }
    // if (cur == MenuCursorPosition::UNKNOWN){
    //     OperationFailedExceptionWithScreenshot::fire(
    //         ErrorReportMode::SEND_ERROR_REPORT,
    //         "Menu cursor not detected after opening menu.",
    //         stream
    //     );
    // }

    MenuCursorPosition cur = MenuCursorPosition::UNKNOWN;
    bool menu_confirmed_open = false;
    for (size_t attempt = 1; attempt <= 3; attempt++){
        stream.log("[Open menu] Open and detect attempt " + std::to_string(attempt) + "/3.");
        try {
            // A late-opening menu may already be visible after a timeout.
            // Check before pressing X again, since X would close that menu.
            if (!menu_confirmed_open){
                MenuWatcher already_open;
                if (wait_until(stream, context, 300ms, {{already_open}}) < 0){
                    overworld_to_menu(stream, context);
                }else{
                    stream.log("[Open menu] Menu already visible; skipping X.");
                }
                menu_confirmed_open = true;
            }else{
                // Cursor detection retries send no input that could close the
                // confirmed menu. Keep observing it instead of toggling X.
                stream.log("[Open menu] Retrying cursor detection in the open menu.");
            }
            pbf_wait(context, 1000ms);
            context.wait_for_all_requests();
            for (int frame = 0; frame < 10 && cur == MenuCursorPosition::UNKNOWN; frame++){
                context.wait_for(50ms);
                const VideoSnapshot snapshot = stream.video().snapshot();
                if (snapshot){
                    cur = detector.detect(snapshot);
                }
            }
            if (cur != MenuCursorPosition::UNKNOWN){
                stream.log("[Open menu] Menu and cursor detected on attempt " + std::to_string(attempt) + ".",
                    COLOR_GREEN);
                break;
            }
            stream.log("[Open menu] Menu cursor not detected on attempt " + std::to_string(attempt) + ".",
                COLOR_ORANGE);
        } catch (OperationFailedException& error){
            stream.log("[Open menu] Attempt " + std::to_string(attempt) + " failed: " + error.message(), COLOR_ORANGE);
        }
    }
    if (cur == MenuCursorPosition::UNKNOWN){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Menu or cursor detection failed after 3 opening attempts.",
            stream);
    }

    IconGrid cur_g = icon_to_grid(cur, num_icons);
    IconGrid tgt_g = icon_to_grid(target, num_icons);
    if (tgt_g.row == -1){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Unknown target menu icon.",
            stream);
    }

    if (cur_g.row != tgt_g.row){
        DpadPosition vert = (cur_g.row < tgt_g.row) ? DPAD_DOWN : DPAD_UP;
        pbf_press_dpad(context, vert, 80ms, 300ms);
        context.wait_for_all_requests();
    }

    int col_diff = tgt_g.col - cur_g.col;
    if (col_diff != 0){
        DpadPosition horiz = (col_diff > 0) ? DPAD_RIGHT : DPAD_LEFT;
        for (int i = 0; i < std::abs(col_diff); i++){
            pbf_press_dpad(context, horiz, 80ms, 300ms);
            context.wait_for_all_requests();
        }
    }

    pbf_press_button(context, BUTTON_A, 80ms, 200ms);
}



}
}
}
