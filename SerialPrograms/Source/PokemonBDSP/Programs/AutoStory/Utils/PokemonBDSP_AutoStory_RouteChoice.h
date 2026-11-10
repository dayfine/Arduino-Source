/*  BDSP AutoStory - Route Choice
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Picks among plans for a walk by the time the shared route scorer
 *  (Pokemon_RouteScorer.h) expects them to take on the console: the plan
 *  the step's costs give, whose open legs are tapped, and plans whose open
 *  legs are held where a long or short hold is safe. Also the scorer's view
 *  of the story maps and of RouteWalker's walking, for the route bench.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteChoice_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteChoice_H

#include <functional>
#include <optional>
#include <vector>
#include "Pokemon/Navigation/Pokemon_RouteScorer.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The scorer's view of `world`: its step rules (a door's tile is stepped
//  onto), its doors, tall grass and wanderers, and `triggers`, tiles where a
//  scene starts and stops the player.
Pokemon::ScoreMap route_score_map(const StoryWorld& world, std::vector<Pokemon::TilePos> triggers = {});

//  RouteWalker's paces: the measured walking and running times, a tap of
//  walk_tiles_dpad(), and the still screen a stop at a wall is confirmed by.
Pokemon::RoutePaces route_score_paces();

//  `legs` as RouteWalker walks them at `speed`: legs to a wall and held
//  legs held, other open legs tapped.
std::vector<Pokemon::ScoredLeg> route_scored_legs(const std::vector<PlannedLeg>& legs, MoveSpeed speed);

//  A walk to score: `legs` from any of `from` toward `goal`, at `speed`.
//  Recoveries are planned as walk_route() first plans them, with `costs`.
struct ScoredRoute{
    const StoryWorld& world;
    std::vector<Pokemon::TilePos> from;
    std::function<bool(const Pokemon::TilePos&)> goal;
    RobustPlanCosts costs;
    MoveSpeed speed = MoveSpeed::WALK;
    std::vector<Pokemon::TilePos> triggers = {};
};
//  The costs walk_route() plans again with after an interruption: `costs`,
//  with open legs dear in tall grass (RobustPlanCosts::per_open_grass_tile),
//  so the new plan crosses the grass on legs to a wall, whose battles need
//  no plan, rather than back on the open legs that lost the count.
RobustPlanCosts replan_costs(const RobustPlanCosts& costs);

//  How `legs` score walked as `route` says, recoveries planned with
//  replan_costs(route.costs).
Pokemon::RouteScore score_planned_route(const ScoredRoute& route, const std::vector<PlannedLeg>& legs);

//  Where an open leg that something stopped may have ended, from `stopped`,
//  the tiles its count of taps gives: those, and the tile before and after
//  each where `world` lets us stand. The count can be a tile off: on lab-B
//  (2026-11-09 18:50, 00.04) "south 33 (open)" was blocked after 22 tiles,
//  counted at Twinleaf (15, 11); the plan from that one tile, "west 1 (to
//  wall)", walked six tiles west, and the rest of the walk ended beside the
//  house. Planned from the wider set, the walk must re-localize on a wall
//  that stops every one of them.
std::vector<Pokemon::TilePos> blocked_stop_tiles(
    const StoryWorld& world, const std::vector<Pokemon::TilePos>& stopped, Pokemon::Direction direction
);

//  The plan for `route`: of plan_recovered_route() with its costs and with
//  a few others (open legs held, costs shaped like the time, open legs
//  dear in grass), the one the scorer passes in the least expected time.
//  If the plan with the route's own costs passes, the one picked is no
//  worse than it in worst time, places to be interrupted or replans, nor
//  walks back more (tiles opposite to an earlier leg; a planned stop at a
//  wall is not backtracking) unless that saves 5 s. The first if none
//  passes; nothing if none plans.
std::optional<std::vector<PlannedLeg>> plan_fastest_route(const ScoredRoute& route);

//  The plan walk_route() walks: a step's `first` plan is
//  plan_fastest_route()'s; a plan after an interruption is
//  plan_recovered_route()'s with replan_costs(), planned once. Scoring
//  every candidate on each of a fault sweep's many replans made the sims
//  ten times slower.
std::optional<std::vector<PlannedLeg>> plan_walk(const ScoredRoute& route, bool first);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
