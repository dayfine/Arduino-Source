/*  Overworld Robust Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Single-map walk route planning using the shared robust leg planner
 *  (Pokemon_RobustLegPlanner.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldRobustRoute_H
#define PokemonAutomation_PokemonFRLG_OverworldRobustRoute_H

#include <functional>
#include <optional>
#include <vector>
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldRouteCosts.h"
#include "PokemonFRLG_OverworldRouteSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Walk leg summary passed to RobustRouteOps::build_step.
struct RobustLegSpec{
    PlayerState start;
    PlayerState end;
    Direction direction = Direction::NORTH;
    int tiles = 0;
    bool to_wall = false;
    bool wild = false;
};

//  Single-map step and leg conversion callbacks supplied by OverworldRoute.cpp.
struct RobustRouteOps{
    std::function<std::optional<PlayerState>(const PlayerState&, Direction)> walk_step;
    std::function<bool(const PlayerState&, Direction)> held_at;
    std::function<RouteStep(const RobustLegSpec&)> build_step;
};

//  Taps the open legs before each tapped leg in `route`, back to the start
//  or a leg that ends where the walk knows the player is (a wall, a warp),
//  so a leg onto stairs from the side starts from a known tile (Pokemon
//  Tower 2F's "west 12" before "south 4" onto the stairs (4, 10)).
void tap_from_known_tile(std::vector<RouteStep>& route);

//  Plans a single-map walk route using Pokemon::plan_robust_legs() when
//  applicable (on land, with no trainer sight/line costs), or nullopt to
//  fall back to the standard leg planner.
std::optional<std::vector<RouteStep>> plan_robust_route_on(
    const MapData& map,
    const PlayerState& first,
    const RouteGoal& goal,
    const RouteCosts& costs,
    std::optional<Direction> start_facing,
    const RobustRouteOps& ops
);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
