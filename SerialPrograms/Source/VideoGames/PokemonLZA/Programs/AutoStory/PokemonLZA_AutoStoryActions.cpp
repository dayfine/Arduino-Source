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
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_RunFromBattleDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
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

void throw_not_implemented(ConsoleHandle& console, std::string_view action, std::string_view todo){
    const std::string message = std::format("PLZA AutoStory: {} is not implemented yet. TODO: {}", action, todo);
    console.log(message, COLOR_RED);
    throw StoryActionNotImplemented(message);
}


void fight_battle(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai
){
    RunFromBattleWatcher battle_menu(COLOR_GREEN, &console.overlay(), 10ms);
    OverworldPartySelectionWatcher overworld(COLOR_WHITE, &console.overlay(), 100ms);
    TrainerBattleState battle_state(battle_ai);
    //  The battle's text first: the overworld only ends it once it has
    //  begun, and it must begin within a minute.
    const WallClock start_deadline = current_time() + 60s;
    bool started = false;
    while (true){
        context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& context){
                pbf_mash_button(context, BUTTON_B, 120s);
            },
            {battle_menu, overworld}
        );
        if (ret == 0){
            started = true;
            if (!battle_state.attempt_one_attack(env, console, context)){
                pbf_press_button(context, BUTTON_UP, 500ms, 1000ms);
            }
            continue;
        }
        if (ret == 1 && started){
            return;
        }
        if (ret < 0 || current_time() > start_deadline){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "fight_battle(): no battle began within a minute, or its menu was gone for two.",
                console
            );
        }
        //  The overworld before the battle: its text has not started it yet.
        context.wait_for(500ms);
    }
}



void fight_battle_in_progress(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, std::chrono::seconds timeout
){
    TrainerBattleState battle_state(battle_ai);
    FlatWhiteDialogDetector white_dialog;
    BlueDialogDetector blue_dialog;
    TealDialogDetector teal_dialog;
    const WallClock deadline = current_time() + timeout;
    while (true){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        if (white_dialog.detect(screen) || blue_dialog.detect(screen) || teal_dialog.detect(screen)){
            return;
        }
        if (current_time() > deadline){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "fight_battle_in_progress(): no dialog after the battle within the timeout.",
                console
            );
        }
        battle_state.attempt_one_attack(env, console, context);
    }
}

void heal_at_pokemon_center(ConsoleHandle& console, ProControllerContext&){
    throw_not_implemented(
        console, "heal_at_pokemon_center",
        "talk to the nurse: no LZA program heals at a Pokemon Center yet."
    );
}

void buy_items(ConsoleHandle& console, ProControllerContext&, const std::vector<std::string>& items){
    throw_not_implemented(
        console, std::format("buy_items ({} items)", items.size()),
        "the Center counter's shop menu. StallBuyer's purchase loop is for stalls and lives in that program."
    );
}


void save_story_game(ConsoleHandle& console, ProControllerContext& context){
    if (!save_game_to_menu(console, context)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "save_story_game(): the game did not save.",
            console
        );
    }
    mash_to_overworld(console, context, BUTTON_B, 10s);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
