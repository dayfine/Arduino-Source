/*  AutoStory Route Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "PokemonFRLG_AutoStoryRoutePose.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::optional<MapPosition> route_start(const std::optional<Pokemon::GridPose>& pose){
    if (!pose || pose->map.empty() || pose->tiles.size() != 1){
        return std::nullopt;
    }
    return MapPosition{pose->map, pose->tiles[0]};
}

std::optional<Pokemon::GridPose> pose_after_failed_route(const RouteProgress& progress){
    if (!progress.at){
        return std::nullopt;
    }
    return Pokemon::GridPose{
        .map = progress.at->map,
        .tiles = {progress.at->point},
        .facing = std::nullopt,
    };
}

void walk_keeping_state(FrlgRunState& state, const RouteProgress* progress, const std::function<void()>& walk){
    try{
        walk();
    }catch (OperationFailedException&){
        state.pose = progress != nullptr ? pose_after_failed_route(*progress) : std::nullopt;
        if (progress != nullptr){
            state.gates = progress->gates;
        }
        throw;
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
