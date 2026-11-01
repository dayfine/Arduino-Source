/*  BDSP AutoStory - Plan Costs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The costs the story's walks are planned with on the BDSP maps
 *  (Pokemon::plan_robust_route()), apart from the walker, so the map
 *  tests plan each walk the way the program does.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_PlanCosts_H
#define PokemonAutomation_PokemonBDSP_AutoStory_PlanCosts_H

#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The default: open legs are whole tiles on the D-pad, so they land exactly
//  at any length; a wall at a leg's end is still worth a little, and each
//  open tile a little more.
Pokemon::RobustPlanCosts route_plan_costs();

//  Between two corners of a walked line: whole tiles on the D-pad land
//  exactly, so an open leg costs nothing extra and the straight leg to the
//  corner beats a run to a wall and a tap back (r244a: "west 10 to the
//  wall, east 1" through the wandering Youngster lost the count; "west 9"
//  in taps keeps it).
Pokemon::RobustPlanCosts waypoint_plan_costs();

//  In town: a one-tile step lands exactly, a short open leg a tile either
//  way, and both cost little more than a leg to a wall, so a plan steps a
//  tile across and walks straight through a gap rather than going round the
//  town to end every leg on a wall (Sandgem: 13 legs, r127a and r128b
//  walked into the Lab and the Mart). Pair it with a check at the end (a
//  sign read) for the timed legs.
Pokemon::RobustPlanCosts town_plan_costs();



}
}
}
#endif
