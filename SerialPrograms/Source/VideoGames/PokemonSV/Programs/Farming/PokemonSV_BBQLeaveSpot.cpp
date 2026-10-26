/*  BBQ Leave Spot
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "PokemonSV_BBQLeaveSpot.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


std::string leave_result_to_string(LeaveResult result){
    switch (result){
    case LeaveResult::FLEW:             return "flew";
    case LeaveResult::STAYED:           return "stayed";
    case LeaveResult::STUCK_IN_BATTLE:  return "stuck in battle";
    }
    return "?";
}

namespace{

//  One try at the fly. BATTLE: a battle interrupted it and is still on screen.
LeaveStep try_fly(LeaveSpotActions& actions){
    LeaveStep step = actions.open_map();
    if (step != LeaveStep::DONE){
        return step;
    }
    step = actions.fly_to_closest_fast_travel();
    if (step != LeaveStep::NOT_FOUND){
        return step;
    }
    return actions.fly_to_closest_pokecenter(2);
}

}

bool recover_to_overworld(Logger& logger, LeaveSpotActions& actions, size_t max_battles){
    for (size_t battles = 0; ; battles++){
        const LeaveStep step = actions.back_to_overworld();
        if (step == LeaveStep::DONE){
            return true;
        }
        if (step != LeaveStep::BATTLE){
            logger.log("Could not get back to the overworld.", COLOR_RED);
            return false;
        }
        if (battles >= max_battles){
            logger.log("Still in a battle after fleeing " + std::to_string(max_battles) + ".", COLOR_RED);
            return false;
        }
        logger.log("A battle is on screen. Fleeing.", COLOR_ORANGE);
        if (!actions.flee()){
            logger.log("Could not flee the battle.", COLOR_RED);
            return false;
        }
    }
}

LeaveResult leave_spot(Logger& logger, LeaveSpotActions& actions, size_t max_attempts){
    for (size_t attempt = 1; attempt <= max_attempts; attempt++){
        const std::string prefix = "Leaving the spot, attempt "
            + std::to_string(attempt) + "/" + std::to_string(max_attempts) + ": ";
        LeaveStep step = try_fly(actions);
        if (step == LeaveStep::DONE){
            logger.log(prefix + "flew to the closest fly point.");
            return LeaveResult::FLEW;
        }
        if (step != LeaveStep::BATTLE){
            //  Nothing to fly to: back out of the map. A battle can start here too.
            step = actions.back_to_overworld();
            if (step != LeaveStep::BATTLE){
                logger.log(prefix + "no fly point found. Leaving from here.", COLOR_ORANGE);
                return LeaveResult::STAYED;
            }
        }
        logger.log(prefix + "a battle started. Fleeing.", COLOR_ORANGE);
        if (!actions.flee()){
            logger.log(prefix + "could not flee the battle.", COLOR_RED);
            return LeaveResult::STUCK_IN_BATTLE;
        }
    }
    logger.log("Leaving the spot: a battle interrupted every attempt. Leaving from here.", COLOR_ORANGE);
    return LeaveResult::STAYED;
}


}
}
}
