/*  AutoStory Route Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a story route starts, and where one that failed leaves the run.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRoutePose_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRoutePose_H

#include <functional>
#include <optional>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where a route from `pose` starts: its tile, or nothing for a set of tiles.
std::optional<MapPosition> route_start(const std::optional<Pokemon::GridPose>& pose);

//  Where a route that failed left the player: the tile it last knew them on,
//  facing not known; nullopt if it failed while moving them.
std::optional<Pokemon::GridPose> pose_after_failed_route(const RouteProgress& progress);

//  Runs `walk`. If it throws OperationFailedException, sets `state`'s pose
//  and gates to where `progress` says the route left them, and rethrows.
void walk_keeping_state(FrlgRunState& state, const RouteProgress* progress, const std::function<void()>& walk);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
