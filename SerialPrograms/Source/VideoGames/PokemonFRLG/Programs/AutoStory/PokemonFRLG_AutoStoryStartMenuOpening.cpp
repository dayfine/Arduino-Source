/*  AutoStory Start Menu Opening
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStartMenuOpening.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


StartOpenAction StartMenuOpening::next(StartOpenScreen screen){
    switch (screen){
    case StartOpenScreen::START_MENU:
        return StartOpenAction::OPENED;
    case StartOpenScreen::BATTLE:
        return StartOpenAction::FAIL_BATTLE;
    case StartOpenScreen::BOX:
        return StartOpenAction::CLOSE_BOX;
    case StartOpenScreen::QUIET_OVERWORLD:
        if (m_presses >= MAX_PRESSES){
            return StartOpenAction::FAIL_NO_MENU;
        }
        m_presses++;
        return StartOpenAction::PRESS_START;
    case StartOpenScreen::SETTLING:
        return StartOpenAction::WAIT;
    }
    return StartOpenAction::WAIT;
}

namespace{

//  How long the overworld shows nothing before START: past a warp's
//  fade-in, as after a battle's black (MovementMechanics::AFTER_BATTLE_QUIET).
constexpr Milliseconds START_QUIET = MovementMechanics::AFTER_BATTLE_QUIET;
//  One wait for that quiet; a look that hasn't settled by then is judged
//  as it is, and the next one waits again.
constexpr Milliseconds START_SETTLE_TIMEOUT = 3000ms;
//  All of it: two scenes' worth of settling, and the presses.
constexpr Milliseconds START_OPEN_TIMEOUT = 20000ms;
//  From START to the menu drawn.
constexpr Milliseconds START_MENU_WATCH = 2000ms;

StartOpenScreen read_start_open_screen(const Pokemon::ScreenSettle& settle, const ImageViewRGB32& screen){
    if (screen && StartMenuDetector(COLOR_RED).detect(screen)){
        return StartOpenScreen::START_MENU;
    }
    if (settle.showing.has(ScreenEvent::BATTLE_STARTED)){
        return StartOpenScreen::BATTLE;
    }
    if (settle.showing.has(ScreenEvent::DIALOG_BOX) || settle.showing.has(ScreenEvent::CHOICE_BOX)){
        return StartOpenScreen::BOX;
    }
    if (quiet_overworld(settle, START_QUIET)){
        return StartOpenScreen::QUIET_OVERWORLD;
    }
    return StartOpenScreen::SETTLING;
}

//  START, watched for the menu.
bool press_start(ConsoleHandle& console, ProControllerContext& context){
    StartMenuWatcher start_menu(COLOR_RED);
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_press_button(context, BUTTON_PLUS, /*hold=*/200ms, /*release=*/START_MENU_WATCH);
        },
        {start_menu}
    );
    return ret == 0;
}

} // namespace

void open_start_menu_settled(ConsoleHandle& console, ProControllerContext& context){
    ConsoleScreenLook eyes(console, context);
    StartMenuOpening opening;
    const WallClock deadline = current_time() + START_OPEN_TIMEOUT;
    while (current_time() < deadline){
        const Pokemon::ScreenSettle settle = wait_for_quiet_overworld(
            [&eyes](bool fresh){ return eyes.look(fresh); }, START_QUIET, START_SETTLE_TIMEOUT
        );
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        switch (opening.next(read_start_open_screen(settle, screen))){
        case StartOpenAction::OPENED:
            console.log("Start menu opened.");
            return;
        case StartOpenAction::WAIT:
            console.log(std::format("START menu: not yet, {}.", Pokemon::describe(settle)));
            break;
        case StartOpenAction::CLOSE_BOX:
            console.log(std::format("START menu: a box first, {}: B.", Pokemon::describe(settle)), COLOR_ORANGE);
            pbf_press_button(context, BUTTON_B, /*hold=*/160ms, /*release=*/500ms);
            break;
        case StartOpenAction::PRESS_START:
            if (press_start(console, context)){
                console.log("Start menu opened.");
                return;
            }
            console.log(std::format("START menu: START on {} opened nothing.", Pokemon::describe(settle)), COLOR_ORANGE);
            break;
        case StartOpenAction::FAIL_BATTLE:
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT, "START menu: a battle shows, not the overworld.", console
            );
        case StartOpenAction::FAIL_NO_MENU:
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::format("START menu: {} START presses on a quiet overworld opened nothing.", StartMenuOpening::MAX_PRESSES),
                console
            );
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("START menu: no quiet overworld in {} s.", START_OPEN_TIMEOUT.count() / 1000),
        console
    );
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
