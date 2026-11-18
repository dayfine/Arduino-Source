/*  AutoStory Route Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a story route starts, and where one that failed leaves the run:
 *  a step tried again after a failure (an interrupt cleared after it)
 *  plans from there, never from where the failed walk started.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRoutePose_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRoutePose_H

#include <functional>
#include <optional>
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecovery.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where a ROUTE from `pose` starts: its tile, or nothing for a set of
//  tiles (paced_pose()), which the route's first holds bring together.
std::optional<MapPosition> route_start(const Pokemon::Pose& pose);

//  Where a route that failed left the player: the tile it last knew them
//  on, facing not known; no pose if it failed while moving them.
Pokemon::Pose pose_after_failed_route(const RouteProgress& progress);

//  Runs `walk`. If it throws OperationFailedException, sets `state`'s pose
//  and gates to where `progress` says the route left them (no progress, a
//  LEGS walk whose legs fit only its start: no pose), and rethrows.
void walk_keeping_state(Pokemon::RunState& state, const RouteProgress* progress, const std::function<void()>& walk);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
