/*  PokemonLZA AutoStory - Battles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogDetector.h"
#include "PokemonLZA_AutoStoryBattles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


void fight_story_battle(
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
                "fight_story_battle(): no dialogue after the battle within the timeout.",
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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
