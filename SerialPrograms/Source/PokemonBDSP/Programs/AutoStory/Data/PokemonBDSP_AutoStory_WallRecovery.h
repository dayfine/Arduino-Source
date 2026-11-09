/*  BDSP Wall Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's movement adapter for shared native wall convergence. Predictions
 *  do not confirm controller execution; the walker must establish arrival.
 */
#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WallRecovery_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WallRecovery_H

#include "Pokemon/Navigation/Pokemon_WallRecovery.h"
#include "PokemonBDSP_AutoStory_RoutePlanner.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

//  Safe holds for every possible position, within one explicitly placed map.
//  Never anchors against a person, crosses a door/map edge or hops a ledge.
//  Caller must uncover trainer sight/scene hazards absent from StoryWorld.
std::vector<Pokemon::WallAnchor<Pokemon::TilePos, TileLeg>> wall_recovery_candidates(
    const StoryWorld& world, const std::vector<Pokemon::TilePos>& from,
    const Pokemon::WallRecoveryLimits& limits = Pokemon::WallRecoveryLimits()
);

//  Every legal prefix, including zero movement, of an interrupted counted leg.
std::vector<Pokemon::TilePos> interrupted_leg_positions(
    const StoryWorld& world, const std::vector<Pokemon::TilePos>& from,
    Pokemon::Direction direction, size_t tiles
);

//  Establish a common wall anchor before continuing from an uncertain pose.
//  Single positions retain the existing planner. Ambiguous positions without
//  a safe anchor fail instead of choosing one. Goals already held need no walk.
//  Of the anchors, the one whose whole walk, holds and the way on to the
//  goal, costs least (robust_route_cost()).
std::optional<std::vector<PlannedLeg>> plan_recovered_route(
    const StoryWorld& world, const std::vector<Pokemon::TilePos>& from,
    const std::function<bool(const Pokemon::TilePos&)>& goal,
    const RobustPlanCosts& costs
);

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
