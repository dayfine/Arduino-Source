/*  BBQ Leave Spot
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  After a quest in the field, fly to the closest fly point so the spot
 *  reloads through the day skip. The game steps are behind an interface so
 *  the flow, including wild battles at any step, is unit tested.
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQLeaveSpot_H
#define PokemonAutomation_PokemonSV_BBQLeaveSpot_H

#include <cstddef>
#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


enum class LeaveStep{
    DONE,       //  The step did what it was asked: the map opened, or the fly landed.
    NOT_FOUND,  //  Nothing to fly to on the map.
    BATTLE,     //  A wild battle is on screen.
    FAILED,     //  Anything else went wrong.
};

//  The game steps leave_spot() drives. They report a wild battle or a failure
//  instead of throwing.
class LeaveSpotActions{
public:
    virtual ~LeaveSpotActions() = default;

    virtual LeaveStep open_map() = 0;
    //  With the map open: the closest fast-travel icon (Terarium fly points).
    virtual LeaveStep fly_to_closest_fast_travel() = 0;
    //  With the map open: the closest Pokecenter, up to `tries` searches.
    virtual LeaveStep fly_to_closest_pokecenter(size_t tries) = 0;
    //  Back to the overworld from the map.
    virtual LeaveStep back_to_overworld() = 0;
    //  Run from the battle on screen. False if it couldn't.
    virtual bool flee() = 0;
};

enum class LeaveResult{
    FLEW,               //  Landed on the closest fly point.
    STAYED,             //  No fly point found, or battles kept interrupting: leave from here.
    STUCK_IN_BATTLE,    //  A battle could not be fled.
};
std::string leave_result_to_string(LeaveResult result);

//  Fly to the closest fly point: the fast-travel icon first, then the
//  closest Pokecenter (2 searches). A battle at any step (a sneak-up battle
//  that starts late, or a wild Pokemon) is fled and the fly tried again, up
//  to `max_attempts` times.
LeaveResult leave_spot(Logger& logger, LeaveSpotActions& actions, size_t max_attempts = 3);

//  Get to the overworld from whatever a stopped run left on screen: the map
//  (R17 19:27) or a wild battle (R17 19:49), fleeing up to `max_battles`
//  battles. False if a battle could not be fled or the overworld never came.
bool recover_to_overworld(Logger& logger, LeaveSpotActions& actions, size_t max_battles = 3);


}
}
}
#endif
