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
    //  overworld_to_menu() reads the menu with the NEW tags' columns, which a new
    //  Pokemon in the party breaks (r19: three misses, then the blind save).
    pbf_press_button(context, BUTTON_X, 160ms, 840ms);
    context.wait_for_all_requests();
    MenuWatcher menu(COLOR_RED, true);
    if (wait_until(stream, context, std::chrono::seconds(10), {{menu}}) < 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Menu not detected after 10 seconds.", stream);
    }
    context.wait_for(100ms);
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

enum class SaveOutcome{ SAVED, WRONG_PLACE, FAILED };
struct SaveAttempt{
    SaveOutcome outcome = SaveOutcome::FAILED;
    std::string wrong_place;
};

//  One feedback-checked save: the save prompt, its card checked against
//  `expected_location` if given, then the save and back to the overworld.
SaveAttempt save_once(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& expected_location, size_t attempt
){
    try{
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
            SaveAttempt wrong{SaveOutcome::WRONG_PLACE,
                "About to save in \"" + location + "\", expected " + expected_location + "; not saving."};
            env.console.log(wrong.wrong_place, COLOR_RED);
            leave_save_prompt(env.console, context);
            return wrong;
        }
        pbf_press_button(context, BUTTON_ZL, 80ms, 5000ms);

        // Return to overworld cleanly.
        pbf_mash_button(context, BUTTON_B, 1000ms);

        // Drain R/confirm/B before detection; an earlier overworld frame
        // must not report success while saving commands are still queued.
        context.wait_for_all_requests();

        // Verify overworld. Generous timeout: save animation + fade can be slow.
        OverworldWatcher overworld;
        if (wait_until(env.console, context, std::chrono::seconds(15), {overworld}) < 0){
            throw OperationFailedExceptionWithScreenshot(ErrorReportMode::SEND_ERROR_REPORT,
                "Failed to detect overworld after save.",
                env.console);
        }

        env.console.log("Checkpoint save verified.");
        return {SaveOutcome::SAVED, ""};
    }catch (OperationFailedException& e){
        env.console.log("Checkpoint save failed: " + e.message(), COLOR_RED);
        context.wait_for(1000ms);
        return {SaveOutcome::FAILED, ""};
    }
}

//  The X menu's icons as hovered, where packaged: the cursor detector's
//  template match. Without them the dark-pixel detector still works.
void load_menu_icon_references(MenuCursorDetector& detector){
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
    const std::string ref_dir = RESOURCE_PATH() + "PokemonBDSP/Autostory";
    for (int i = 0; i < MENU_CURSOR_NUM_ICONS; i++){
        const std::string path = ref_dir + "/" + ICON_NAMES[i] + ".png";
        if (!Filesystem::is_regular_file(path)){
            continue;
        }
        ImageRGB32 img(path);
        if (img){
            detector.set_icon_reference(static_cast<MenuCursorPosition>(i), std::move(img));
        }
    }
}

//  The cursor on the open menu, from up to ten frames; UNKNOWN if none reads.
MenuCursorPosition read_menu_cursor(VideoStream& stream, ProControllerContext& context, const MenuCursorDetector& detector){
    MenuCursorPosition cur = MenuCursorPosition::UNKNOWN;
    for (int frame = 0; frame < 10 && cur == MenuCursorPosition::UNKNOWN; frame++){
        context.wait_for(50ms);
        const VideoSnapshot snapshot = stream.video().snapshot();
        if (snapshot){
            cur = detector.detect(snapshot);
        }
    }
    return cur;
}

//  Opens the menu unless it is already open: a late-opening menu may show
//  after a timeout, and X would close it.
void open_menu_unless_open(VideoStream& stream, ProControllerContext& context){
    MenuWatcher already_open;
    if (wait_until(stream, context, 300ms, {{already_open}}) < 0){
        overworld_to_menu(stream, context);
    }else{
        stream.log("[Open menu] Menu already visible; skipping X.");
    }
}

//  Opens the menu and reads its cursor, in three attempts. Once the menu
//  is confirmed open, retries only look: no input that could close it.
MenuCursorPosition open_menu_and_find_cursor(VideoStream& stream, ProControllerContext& context, const MenuCursorDetector& detector){
    MenuCursorPosition cur = MenuCursorPosition::UNKNOWN;
    bool menu_confirmed_open = false;
    for (size_t attempt = 1; attempt <= 3 && cur == MenuCursorPosition::UNKNOWN; attempt++){
        stream.log("[Open menu] Open and detect attempt " + std::to_string(attempt) + "/3.");
        try{
            if (!menu_confirmed_open){
                open_menu_unless_open(stream, context);
                menu_confirmed_open = true;
            }else{
                stream.log("[Open menu] Retrying cursor detection in the open menu.");
            }
            pbf_wait(context, 1000ms);
            context.wait_for_all_requests();
            cur = read_menu_cursor(stream, context, detector);
            stream.log("[Open menu] Menu cursor " + std::string(cur == MenuCursorPosition::UNKNOWN ? "not detected" : "detected") +
                " on attempt " + std::to_string(attempt) + ".", cur == MenuCursorPosition::UNKNOWN ? COLOR_ORANGE : COLOR_GREEN);
        }catch (OperationFailedException& error){
            stream.log("[Open menu] Attempt " + std::to_string(attempt) + " failed: " + error.message(), COLOR_ORANGE);
        }
    }
    if (cur == MenuCursorPosition::UNKNOWN){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Menu or cursor detection failed after 3 opening attempts.",
            stream);
    }
    return cur;
}

//  From the icon `cur` to `target`: a row up or down, then across.
void move_menu_cursor(VideoStream& stream, ProControllerContext& context, MenuCursorPosition cur, MenuCursorPosition target, int num_icons){
    const IconGrid cur_g = icon_to_grid(cur, num_icons);
    const IconGrid tgt_g = icon_to_grid(target, num_icons);
    if (tgt_g.row == -1){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Unknown target menu icon.",
            stream);
    }
    if (cur_g.row != tgt_g.row){
        pbf_press_dpad(context, cur_g.row < tgt_g.row ? DPAD_DOWN : DPAD_UP, 80ms, 300ms);
        context.wait_for_all_requests();
    }
    const int col_diff = tgt_g.col - cur_g.col;
    for (int i = 0; i < std::abs(col_diff); i++){
        pbf_press_dpad(context, col_diff > 0 ? DPAD_RIGHT : DPAD_LEFT, 80ms, 300ms);
        context.wait_for_all_requests();
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
){
    static constexpr size_t MAX_RETRIES = 3;

    for (size_t attempt = 0; attempt < MAX_RETRIES; attempt++){
        const SaveAttempt result = save_once(env, context, expected_location, attempt);
        if (result.outcome == SaveOutcome::SAVED){
            return true;
        }
        //  No blind fallback for the wrong place: it would save there.
        if (result.outcome == SaveOutcome::WRONG_PLACE){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, result.wrong_place, env.console);
        }
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
    context.wait_for_all_requests();
    MenuCursorDetector detector;
    load_menu_icon_references(detector);
    const MenuCursorPosition cur = open_menu_and_find_cursor(stream, context, detector);
    move_menu_cursor(stream, context, cur, target, num_icons);
    pbf_press_button(context, BUTTON_A, 80ms, 200ms);
}



}
}
}
