/*  BDSP AutoStory - Old story steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Steps the story table had that went wrong on the console, as they were:
 *  the story simulation's regression tests run them in place of the
 *  table's step of the same name and check they still go wrong there.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_OldStorySteps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_OldStorySteps_H

#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  r75: out of the Oreburgh Gate as a run west until Route 203's banner
//  (the old event west_until_transition()). The banner reads a few tiles
//  in: the run ends west of the Gate's warp tile (55, 9), in the pocket by
//  the pond (Route 203's x 49 to 54, rows 6 to 13).
Pokemon::StoryStep r75_old_gate_exit();
//  Where r75's run west ended: the pocket by Route 203's pond.
Pokemon::Pose r75_pond_pocket();

//  r81: out of the Ravaged Path as a timed walk south (the old event's
//  4 s walk_dir()), its fade missed: on past cave_exit (20, 26) to the
//  cliff, column 20 down to row 30.
Pokemon::StoryStep r81_old_cave_exit();
//  Where r81's walk south ended: below the Ravaged Path's mouth.
Pokemon::Pose r81_below_the_mouth();

//  Oreburgh Mine B1F: tapping 19 tiles leaves the player 1 tile short of the warp door.
Pokemon::StoryStep old_mine_hall_19(std::string name, Pokemon::Direction direction);

//  Oreburgh Mine: stepping south at col 29 without bypassing the crate
//  hits the wooden crate at (29, 28) on the south leg, a tile down.
Pokemon::StoryStep old_coal_room_crate_cut();

//  01.05's old tap into the Oreburgh Gate, west to its banner, after an
//  open walk to the tile beside its mouth (1, 13) that slid onto the warp:
//  the tap step began inside the Gate (lab-B 2026-11-08 15:31 to 15:54).
Pokemon::StoryStep old_tap_into_the_gate();

//  01.11's old walk toward the old woman: 8 taps west along the row
//  Floaroma's banner stopped us on, from x 10 to 12, so north of it ended
//  below the old man (lab-B 2026-11-08 18:41).
Pokemon::StoryStep old_west_along_the_field();

//  00.04's old walk round Mom to (6, 9) and step south onto the mat (6, 10)
//  for her call: her call fired on (6, 9) at the walk's end, the walk took
//  it for a trainer and pressed through it, the step went out of the door,
//  and her hat scene saw no fade (lab-B 2026-11-09 17:29).
std::vector<Pokemon::StoryStep> old_round_mom_to_the_mat();



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
