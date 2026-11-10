/*  AutoStory Pacing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "PokemonFRLG_AutoStoryPacing.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using OverworldWalkSteps::BikeSelectCalls;


bool select_off_bike(const BikeSelectCalls& calls){
    for (size_t c = 0; c < 2 && calls.riding(); c++){
        calls.press_select();
    }
    return !calls.riding();
}

bool find_paced_encounter(Logger& logger, const BikeSelectCalls& bike, PatchSteps& steps){
    if (!select_off_bike(bike)){
        logger.log("Pacing: still on the Bicycle after SELECT twice; not pacing on it.", COLOR_RED);
        return false;
    }
    return pace_until_battle(logger, steps, PACE_MAX_STEPS);
}

BikeSelectCalls console_bike_select(ConsoleHandle& console, ProControllerContext& context){
    BikeSelectCalls calls;
    calls.press_select = [&console, &context]{
        console.log("Pacing: SELECT (MINUS) off the Bicycle.");
        pbf_press_button(context, BUTTON_MINUS, MovementMechanics::BIKE_SELECT_PRESS, MovementMechanics::BIKE_SELECT_SETTLE);
    };
    calls.riding = [&console, &context]{
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        return screen && OverworldWalkSteps::screen_shows_riding(*screen.frame);
    };
    return calls;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
