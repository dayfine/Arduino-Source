/*  AutoStory Pacing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_BikeSpinPresses.h"
#include "PokemonFRLG_AutoStoryPacing.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldBike.h"

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


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
