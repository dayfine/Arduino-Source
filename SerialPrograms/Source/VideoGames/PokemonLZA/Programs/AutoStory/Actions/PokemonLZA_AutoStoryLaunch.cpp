/*  PokemonLZA AutoStory - Launch from HOME
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/StartupChecks/StartProgramChecks.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"     //  NintendoSwitch_CloseGameDetector.h needs it first.
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_CloseGameDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "PokemonLZA_AutoStoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


LaunchStart read_launch_start(const ImageViewRGB32& frame, bool home_menu_shows, const ScreenCheck& first_screen){
    if (!frame){
        return LaunchStart::NO_VIDEO;
    }
    if (first_screen && first_screen(frame)){
        return LaunchStart::FIRST_SCREEN;
    }
    return home_menu_shows ? LaunchStart::HOME_MENU : LaunchStart::OTHER;
}

LaunchMove next_launch_move(LaunchStart start, bool home_pressed){
    switch (start){
    case LaunchStart::HOME_MENU:
        return LaunchMove::LAUNCH;
    case LaunchStart::FIRST_SCREEN:
    case LaunchStart::OTHER:
        return home_pressed ? LaunchMove::REFUSE : LaunchMove::PRESS_HOME;
    case LaunchStart::NO_VIDEO:
        return LaunchMove::REFUSE;
    }
    return LaunchMove::REFUSE;
}


bool wait_for_screen(ConsoleHandle& console, ProControllerContext& context, const ScreenCheck& check, Milliseconds timeout){
    DetectorToFinder<ScreenCheckDetector> screen("ScreenCheckWatcher", Milliseconds(0), check);
    context.wait_for_all_requests();
    return wait_until(console, context, timeout, {screen}) == 0;
}


namespace{

//  On HOME with the tile selected: X, and if the close dialog shows the
//  game was running, so A to close it and back to HOME. X on a game that is
//  not running does nothing.
void close_selected_game_if_running(ConsoleHandle& console, ProControllerContext& context){
    console.log("launch: X on the selected tile (closes it if it is running).");
    pbf_press_button(context, BUTTON_X, 160ms, 0ms);
    CloseGameDetector close_dialog(console);
    HomeMenuDetector home(console, COLOR_RED);
    const ScreenCheck dialog_shows = [&](const ImageViewRGB32& frame){ return close_dialog.detect(frame); };
    if (!wait_for_screen(console, context, dialog_shows, 3000ms)){
        if (!wait_for_screen(console, context, [&](const ImageViewRGB32& frame){ return home.detect_only(frame); }, 2000ms)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "launch: after X on the selected tile, neither the close dialog nor the HOME menu shows.",
                console
            );
        }
        console.log("launch: no close dialog: the selected game is not running.");
        return;
    }
    console.log("launch: the close dialog: the selected game was running; A to close it.");
    pbf_press_button(context, BUTTON_A, 160ms, 0ms);
    const ScreenCheck home_back = [&](const ImageViewRGB32& frame){
        return home.detect_only(frame) && !close_dialog.detect(frame);
    };
    if (!wait_for_screen(console, context, home_back, 15s)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "launch: the HOME menu did not come back within 15 s of closing the selected game.",
            console
        );
    }
    console.log("launch: the selected game is closed.");
}

//  The screen now, read as the launch step's start.
LaunchStart read_screen(ConsoleHandle& console, ProControllerContext& context, const ScreenCheck& first_screen){
    context.wait_for_all_requests();
    const VideoSnapshot frame = console.video().snapshot();
    HomeMenuDetector home(console, COLOR_RED);
    const bool home_shows = frame && home.detect_only(frame);
    return read_launch_start(frame, home_shows, first_screen);
}

//  Until the HOME menu shows, at most HOME once (see next_launch_move()).
//  Throws UserSetupError if it does not.
void reach_home_menu(ConsoleHandle& console, ProControllerContext& context, std::string_view game, const ScreenCheck& first_screen){
    LaunchStart start = read_screen(console, context, first_screen);
    bool home_pressed = false;
    while (true){
        switch (next_launch_move(start, home_pressed)){
        case LaunchMove::LAUNCH:
            return;
        case LaunchMove::PRESS_HOME:{
            console.log(std::format("launch: not on HOME ({}); HOME once: the game in front stays the selected tile.", (int)start));
            pbf_press_button(context, BUTTON_HOME, 160ms, 0ms);
            HomeMenuDetector home(console, COLOR_RED);
            wait_for_screen(console, context, [&](const ImageViewRGB32& frame){ return home.detect_only(frame); }, 5000ms);
            home_pressed = true;
            start = read_screen(console, context, first_screen);
            break;
        }
        case LaunchMove::REFUSE:
            throw UserSetupError(console, std::format(
                "Start a new game with {} in front, or on the HOME screen with {}'s tile selected. {}",
                game, game, start == LaunchStart::NO_VIDEO ? "There is no video." : "HOME did not show after a HOME press."
            ));
        }
    }
}

} // namespace


void launch_selected_game_from_home(
    ConsoleHandle& console, ProControllerContext& context,
    std::string_view game, std::string_view first_screen_name,
    const ScreenCheck& first_screen, Milliseconds timeout,
    std::string_view dump_folder
){
    reach_home_menu(console, context, game, first_screen);
    //  The program's start skipped this check (the game in front may have
    //  been dark): HOME fills the screen.
    context.wait_for_all_requests();
    StartProgramChecks::check_border(console);
    console.log(std::format("launch: the HOME menu; launching the selected tile ({} expected), the cursor not moved.", game));
    close_selected_game_if_running(console, context);
    console.log("launch: A on the selected tile, then the user select and the license check.");
    start_game_from_home(console, context, true, 0, 0);
    //  The launch's logos: the first screen comes no sooner than this.
    context.wait_for(5000ms);
    if (!wait_for_screen(console, context, first_screen, timeout)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format(
                "launch: the {} did not show within {} ms: the selected game may not be {}, or it has a save.",
                first_screen_name, timeout.count(), game
            ),
            console
        );
    }
    console.log(std::format("launch: the {} shows.", first_screen_name));
    context.wait_for_all_requests();
    const VideoSnapshot shown = console.video().snapshot();
    if (shown){
        std::string label(first_screen_name);
        std::replace(label.begin(), label.end(), ' ', '-');
        dump_debug_image(console, std::string(dump_folder), label, shown);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
