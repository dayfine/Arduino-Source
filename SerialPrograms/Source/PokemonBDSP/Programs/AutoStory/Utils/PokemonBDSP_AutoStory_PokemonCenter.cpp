/*  BDSP AutoStory - Pokemon Center
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_PokemonCenter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


void heal_at_pokemon_center(
    VideoStream& stream, ProControllerContext& context,
    GridDirection door_side, const std::string& label
){
    route_status(stream, label + ": finding the door.");
    bool inside = false;
    for (size_t tries = 0; tries < 5 && !inside; tries++){
        inside = walk_until(stream, context, 0, 1, 1200ms, label + ": into the door").stop == WalkStop::TRANSITION;
        if (!inside){
            route_turn(stream, context, door_side);
            walk_for(stream, context, door_side == GridDirection::EAST ? 1 : -1, 0,
                route_open_hold(door_side, 1), label + ": a tile toward the door");
        }
    }
    if (!inside){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": door not found.", stream);
    }
    pbf_wait(context, 1500ms);

    //  The nurse's counter is straight ahead of the door.
    route_status(stream, label + ": talking to the nurse.");
    if (walk_to_talk(stream, context, 0, 1, 6000ms, label + ": to the nurse").stop != WalkStop::DIALOG){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": nurse not reached.", stream);
    }
    //  A through her lines; the cursor starts on "Yes". Stop once no dialogue
    //  has shown for a while, so A does not start the talk over.
    size_t quiet = 0;
    for (size_t presses = 0; presses < 30 && quiet < 2; presses++){
        ShortDialogWatcher dialog(COLOR_YELLOW);
        if (wait_until(stream, context, 1500ms, {{dialog}}) == 0){
            quiet = 0;
            pbf_press_button(context, BUTTON_A, 80ms, 700ms);
        }else{
            quiet++;
        }
    }
    context.wait_for_all_requests();

    route_status(stream, label + ": healed; walking out.");
    if (walk_until(stream, context, 0, -1, 6000ms, label + ": out the door").stop != WalkStop::TRANSITION){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": did not get back out.", stream);
    }
    pbf_wait(context, 1500ms);
    context.wait_for_all_requests();
}



}
}
}
