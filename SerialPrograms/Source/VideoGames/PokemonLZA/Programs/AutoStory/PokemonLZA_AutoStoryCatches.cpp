/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  A through text and tutorial cards until neither has shown for 3 s
//  (30 s at most). Returns whether a card showed.
bool advance_text_and_cards(ConsoleHandle& console, ProControllerContext& context){
    FlatWhiteDialogDetector white_dialog;
    BlueDialogDetector blue_dialog;
    TealDialogDetector teal_dialog;
    bool card = false;
    WallClock last_busy = current_time();
    const WallClock deadline = last_busy + 30s;
    while (current_time() < deadline && current_time() - last_busy < 3s){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        const bool card_now = screen && tutorial_card_shows(screen);
        if (card_now || white_dialog.detect(screen) || blue_dialog.detect(screen) || teal_dialog.detect(screen)){
            card = card || card_now;
            last_busy = current_time();
            pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        }else{
            context.wait_for(250ms);
        }
    }
    return card;
}

//  ZL+ZR held 1.5 s throws a ball at the locked target (0.5 s only aims),
//  then the ball's flight, shakes and any loading screen.
void throw_ball(ProControllerContext& context){
    pbf_press_button(context, BUTTON_ZL, 500ms, 100ms);
    pbf_press_button(context, BUTTON_ZL | BUTTON_ZR, 1500ms, 6000ms);
    context.wait_for_all_requests();
}

} // namespace


void catch_tutorial(ConsoleHandle& console, ProControllerContext& context, int max_throws){
    for (int i = 0; i < max_throws; i++){
        throw_ball(context);
        if (advance_text_and_cards(console, context)){
            console.log(std::format("catch_tutorial(): caught on throw {}.", i + 1));
            return;
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("catch_tutorial(): no catch in {} throws.", max_throws),
        console
    );
}

void battle_then_catch(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int max_rounds
){
    pbf_press_dpad(context, DPAD_UP, 100ms, 2000ms);
    TrainerBattleState battle_state(battle_ai);
    for (int round = 0; round < max_rounds; round++){
        int landed = 0;
        for (int tries = 0; tries < 12 && landed < 3; tries++){
            landed += battle_state.attempt_one_attack(env, console, context) ? 1 : 0;
        }
        if (landed == 0 && round > 0){
            console.log(std::format("battle_then_catch(): nothing to lock on in round {}: caught.", round + 1));
            return;
        }
        advance_text_and_cards(console, context);
        throw_ball(context);
        advance_text_and_cards(console, context);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("battle_then_catch(): still a target after {} rounds.", max_rounds),
        console
    );
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
