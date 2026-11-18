/*  AutoStory Planned Risk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a League turn risks, as planned.py reads it (PokemonSimulation
 *  #111): the trainer's Full Restore slots, the foe's HP when our move
 *  lands, the HP we can lose this turn and who moves first.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlannedRisk_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlannedRisk_H

#include "PokemonFRLG_AutoStoryPlannedState.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  pret ShouldUseItem's slot gate for a trainer carrying `carried` items
//  of one kind (Full Restores), `used` of them used, with `alive` Pokemon
//  left: slot i > 0 stays shut while more than 4 - i + 1 are alive.
bool trainer_item_open(int carried, int used, int alive);

//  The foe's HP when our move lands: full when its trainer uses the item
//  first (under 1/4 with a slot open).
int foe_hp_at_our_move(const PlannedState& state);

//  The HP we can lose this turn: residual damage plus the foe's worst
//  ordinary hit as it stands (normal_max), and the mean of its best hit.
//  Residual only while it recharges, sleeps, is frozen or uses an item.
struct PlannedDanger{
    int normal_max = 0;
    double mean = 0;
};

//  The danger of `state`'s turn (PlannedDanger).
PlannedDanger planned_danger(const PlannedState& state);

//  `move` lands before the foe's: by priority, its recharge, then Speed.
bool we_move_first(const PlannedState& state, const PlannedMove& move);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
