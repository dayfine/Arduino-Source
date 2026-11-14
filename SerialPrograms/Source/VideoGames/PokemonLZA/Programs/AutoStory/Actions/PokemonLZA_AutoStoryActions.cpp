/*  PokemonLZA AutoStory - Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_MenuNavigation.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_RunFromBattleDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "PokemonLZA_AutoStoryActions.h"
#include "PokemonLZA_AutoStoryDialogs.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


StoryActionNotImplemented::StoryActionNotImplemented(std::string message)
    : OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, std::move(message))
{}


void fight_battle(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai
){
    //  The battle's text first: the overworld only ends it once it has
    //  begun, and it must begin within a minute.
    RunFromBattleWatcher battle_menu(COLOR_GREEN, &console.overlay(), 10ms);
    context.wait_for_all_requests();
    const int started = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 60s);
        },
        {battle_menu}
    );
    if (started < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "fight_battle(): no battle began within a minute.",
            console
        );
    }
    TrainerBattleState battle_state(battle_ai);
    OverworldPartySelectionWatcher overworld(COLOR_WHITE, &console.overlay(), 100ms);
    const int ended = run_battle_until(
        env, console, context, battle_state, overworld, 120s,
        [](ProControllerContext& context){
            pbf_press_button(context, BUTTON_UP, 500ms, 1000ms);
        }
    );
    if (ended < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "fight_battle(): the battle's menu was gone for two minutes.",
            console
        );
    }
}



void fight_battle_in_progress(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, std::chrono::seconds timeout
){
    TrainerBattleState battle_state(battle_ai);
    StoryDialogDetector any_dialog;
    const WallClock deadline = current_time() + timeout;
    bool attacked = false;
    while (true){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        const bool dialog = any_dialog.detect(screen);
        if (dialog && attacked){
            return;
        }
        if (current_time() > deadline){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "fight_battle_in_progress(): no dialog after the battle within the timeout.",
                console
            );
        }
        if (dialog || !battle_state.attempt_one_attack(env, console, context)){
            if (!attacked){
                pbf_press_button(context, BUTTON_A, 160ms, 400ms);
            }
            continue;
        }
        attacked = true;
    }
}



namespace{

//  Whether the report page's Save button (its cursor arrow, where
//  save_game_to_menu() looks for it) shows within `timeout`.
bool save_button_shows(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout){
    //  save_game_to_menu()'s box (PokemonLZA_BasicNavigation.cpp), which it
    //  keeps inline; a copy until upstream names it.
    SelectionArrowWatcher save_button(
        COLOR_GREEN, &console.overlay(), SelectionArrowType::RIGHT, {0.287118, 0.638835, 0.064410, 0.108738}
    );
    return wait_until(console, context, timeout, {save_button}) == 0;
}

//  From the overworld: X opens the menu, R its report page. The first menu
//  of a playthrough opens under a save tutorial card that A closes, leaving
//  the menu with the cursor on the Bag (console B, 2026-11-10); R does
//  nothing over the card. So when R does not bring up the Save button, A
//  once, then R again. A is never pressed on the plain menu, where it
//  would open the Bag.
void open_save_report(ConsoleHandle& console, ProControllerContext& context){
    pbf_press_button(context, BUTTON_X, 160ms, 2340ms);
    pbf_press_button(context, BUTTON_R, 160ms, 240ms);
    if (save_button_shows(console, context, 5s)){
        return;
    }
    console.log("save_story_game(): no Save button after R: the menu's tutorial card; pressing A, then R.");
    pbf_press_button(context, BUTTON_A, 160ms, 1340ms);
    pbf_press_button(context, BUTTON_R, 160ms, 240ms);
    if (!save_button_shows(console, context, 5s)){
        console.log("save_story_game(): still no Save button; save_game_to_menu() takes over.", COLOR_RED);
    }
}

//  save_game_to_menu(), its failures as false.
bool try_save(ConsoleHandle& console, ProControllerContext& context){
    try{
        return save_game_to_menu(console, context);
    }catch (OperationFailedException& e){
        console.log(std::format("save_story_game(): {}", e.message()), COLOR_RED);
        return false;
    }
}

} // namespace

//  Opens the report page (open_save_report(), for the first menu's
//  tutorial), then save_game_to_menu() presses Save once its button shows
//  and waits for the "Saved!" text, so the save is confirmed. If that
//  fails, B back to the overworld (B is harmless while writing) and one
//  more try.
void save_story_game(ConsoleHandle& console, ProControllerContext& context){
    open_save_report(console, context);
    if (!try_save(console, context)){
        mash_to_overworld(console, context, BUTTON_B, 10s);
        if (!save_game_to_menu(console, context)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "save_story_game(): the game did not save.",
                console
            );
        }
    }
    mash_to_overworld(console, context, BUTTON_B, 10s);
}

void claim_mable_research_reward(ConsoleHandle& console, ProControllerContext& context){
    overworld_to_main_menu(console, context);
    for (int i = 0; i < 3; i++){
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 400ms);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 2840ms);

    //  The page opens on Rewards under Mable's lines on the level 2 reward,
    //  TM004 Rock Smash (console B, 2026-11-10): A while a dialog shows,
    //  until none has for 3 s, then B back to the overworld, through her
    //  talk after the menu closes (over 20 s; console B, 2026-11-10).
    if (press_a_until_quiet(console, context, {.item_receive = true}, 3s, 60s).timed_out){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "claim_mable_research_reward(): Mable's lines did not end within 60 s.",
            console
        );
    }
    mash_to_overworld(console, context, BUTTON_B, 60s);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
