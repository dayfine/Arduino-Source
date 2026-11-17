/*  AutoStory Planned Between
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The planned policy's top-ups between two League fights, as planned.py's
 *  between() makes them (PokemonSimulation #111): the next fight's
 *  `between` row of the League plan, its item reserve held.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlannedBetween_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlannedBetween_H

#include <string>
#include <vector>
#include "PokemonFRLG_AutoStoryPlannedState.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Our side between two fights.
struct PlannedCarry{
    int hp = 0;
    int max_hp = 0;
    PlannedStatus status = PlannedStatus::NONE;
    //  PP left and full PP, by move.
    SlugCounts pp = {};
    SlugCounts full_pp = {};
    SlugCounts bag = {};
};

//  The top-ups before `next` (its `between` row): a cure, Super Potions
//  (then a Full Restore) to the HP target, a PP item to the PP floor;
//  `next`'s item reserve holds. Updates `carry`; returns the items used,
//  in order.
std::vector<std::string> planned_between(PlannedCarry& carry, const LeagueFightData& next);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
