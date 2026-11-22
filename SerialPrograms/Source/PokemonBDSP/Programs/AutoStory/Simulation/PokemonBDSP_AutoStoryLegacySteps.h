/*  BDSP AutoStory - Old story steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Steps the story table had that went wrong on the console, as they were:
 *  the story simulation's regression tests run them in place of the
 *  table's step of the same name and check they still go wrong there.
 *
// code-health: allow header_symbols -- legacy story steps definitions for test parity verification
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStoryLegacySteps_H
#define PokemonAutomation_PokemonBDSP_AutoStoryLegacySteps_H

#include <string>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  r75: out of the Oreburgh Gate as a run west until Route 203's banner.
BdspStep r75_old_gate_exit();
//  Where r75's run west ended: the pocket by Route 203's pond.
Pokemon::GridPose r75_pond_pocket();

//  r81: out of the Ravaged Path as a timed walk south.
BdspStep r81_old_cave_exit();
//  Where r81's walk south ended: below the Ravaged Path's mouth.
Pokemon::GridPose r81_below_the_mouth();

//  Oreburgh Mine B1F: tapping 19 tiles leaves the player 1 tile short of the warp door.
BdspStep old_mine_hall_19(std::string name, Pokemon::Direction direction);

//  Oreburgh Mine: stepping south at col 29 without bypassing the crate
//  hits the wooden crate at (29, 28) on the south leg, a tile down.
BdspStep old_coal_room_crate_cut();

//  01.05's old tap into the Oreburgh Gate, west to its banner.
BdspStep old_tap_into_the_gate();

//  01.11's old walk toward the old woman: 8 taps west along the row.
BdspStep old_west_along_the_field();

//  00.04's old walk round Mom to (6, 9) and step south onto the mat (6, 10).
std::vector<BdspStep> old_round_mom_to_the_mat();



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
