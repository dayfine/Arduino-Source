/*  AutoStory Route Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonFRLG_AutoStoryRoutePose.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::optional<MapPosition> route_start(const Pokemon::Pose& pose){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    if (tiles.size() != 1){
        return std::nullopt;
    }
    return MapPosition{pose.map, tiles[0]};
}

Pokemon::Pose pose_after_failed_route(const RouteProgress& progress){
    if (!progress.at){
        return Pokemon::Pose{};
    }
    return Pokemon::tile_pose(progress.at->map, {progress.at->point}, std::nullopt);
}

void walk_keeping_state(Pokemon::RunState& state, const RouteProgress* progress, const std::function<void()>& walk){
    try{
        walk();
    }catch (OperationFailedException&){
        if (progress == nullptr){
            state.pose = Pokemon::Pose{};
            throw;
        }
        state.pose = pose_after_failed_route(*progress);
        state.gates = progress->gates;
        throw;
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
