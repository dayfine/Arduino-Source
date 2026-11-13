/*  Overworld Route Costs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the route planner weighs a route by, and the default costs.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldRouteCosts_H
#define PokemonAutomation_PokemonFRLG_OverworldRouteCosts_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldRouteSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What the planner weighs a route by. Each is a cost per occurrence.
enum class RouteCost{
    TILE,
    TURN,           //  extra for a leg in a new direction
    WARP,           //  the fade in and out
    SURF,           //  the dialog and the animation; below 0, never
    GRASS_TILE,     //  extra for tall grass, where wild Pokemon appear
    //  Extra for a leg that ends where the player could keep walking: it has
    //  to be timed, and if a message or a battle stops it part-way, where
    //  the player is can only be estimated. A leg that ends at a wall is
    //  walked again instead.
    OPEN_STOP,
    //  Extra again when that leg crosses tall grass or water, where a wild
    //  battle is likely to stop it: the walker can't tell where it did
    //  (console B, 2026-11-01, Route 1, four runs).
    OPEN_WILD_STOP,
    //  Extra for a step onto a tile a trainer may see, any way it may look
    //  (trainer_may_see()): a battle the route would rather not start. A
    //  cost, not a ban: a route walks past a trainer when the way round
    //  is much longer.
    SIGHT_TILE,
    //  Extra for an open leg whose tile past its end is one a trainer may
    //  see: a leg that runs a tile on starts the battle (console B,
    //  2026-11-11 15:03, Rocket Hideout B2F: a south 2 to (19, 5) went on
    //  to (19, 6), in the Grunt's line). Below SIGHT_TILE: it takes a
    //  missed stop to cost the battle.
    SIGHT_OVERSHOOT,
    //  Extra for an open leg north or south that ends below or above a
    //  sign: a hold that runs on reads it, and the walk is interrupted
    //  (console B, 2026-11-12, 11.03: "north 5" to Safari Zone West's
    //  (27, 10) on the Bicycle). A tie-break more than a detour.
    SIGN_OVERSHOOT,
    //  Extra for an open leg that ends on the water: open water has hardly
    //  a wall, so a leg stopped part-way there can't be placed, and the
    //  player isn't found again (console B, 2026-11-13 06:55 and 07:02,
    //  12.03: "south 15 (open)" down Route 21 stopped by a message). Above
    //  OPEN_WILD_STOP and a few Surfs: a leg that lands on a sand bar and
    //  is held to its bank finds the player, so Route 21 is crossed sand
    //  bar to sand bar (the user's design, 2026-11-13).
    OPEN_WATER_STOP,
};
//  Cost mapping table for path planning.
using RouteCosts = std::map<RouteCost, int>;

//  TILE 1, TURN 2, WARP 30, SURF 40, GRASS_TILE 20, OPEN_STOP 8,
//  OPEN_WILD_STOP 300, SIGHT_TILE 0, SIGHT_OVERSHOOT 0, SIGN_OVERSHOOT 3,
//  OPEN_WATER_STOP 1000.
//  The sight costs are
//  off by default until beaten trainers and battles a story step wants are
//  told apart: at 100 and 10 they moved Pewter City's and Pewter Gym's
//  plans (the Gym's Camper is fought on the way to Brock).
const RouteCosts& default_route_costs();

//  `costs`'s cost for `kind`, or the default if it has none.
int route_cost(const RouteCosts& costs, RouteCost kind);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
