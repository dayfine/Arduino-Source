/*  AutoStory Paced Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a step that paces (a GRIND or capture) leaves the player, and the
 *  route on from there.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPacedRoute_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPacedRoute_H

#include <optional>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryPoses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where a GRIND or capture `step` that paces (pace = ...) leaves the
//  player who started it on `patch`: the patch's tile or the one its pace
//  leads to, as the last battle came, facing unknown. `patch` for any
//  other step, or a `patch` not on one tile.
Pokemon::Pose paced_pose(const Pokemon::Pose& patch, const StoryStep& step);

//  A ROUTE's legs from `pose` to `goal`: from one tile, as planned; from
//  a set of tiles (paced_pose()), first the holds to walls that bring them
//  all to one tile (holds_to_one_tile()), then the route from there.
//  Nothing if either can't be planned.
std::optional<std::vector<RouteStep>> plan_story_route(
    const std::vector<MapData>& maps, const Pokemon::Pose& pose,
    std::optional<Direction> facing, const RouteGoal& goal
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
