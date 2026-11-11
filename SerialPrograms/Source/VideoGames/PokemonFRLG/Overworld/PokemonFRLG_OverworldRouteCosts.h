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
};
//  Cost mapping table for path planning.
using RouteCosts = std::map<RouteCost, int>;

//  TILE 1, TURN 2, WARP 30, SURF 40, GRASS_TILE 20, OPEN_STOP 8,
//  OPEN_WILD_STOP 300.
const RouteCosts& default_route_costs();

//  `costs`'s cost for `kind`, or the default if it has none.
int route_cost(const RouteCosts& costs, RouteCost kind);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
