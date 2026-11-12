/*  BDSP AutoStory - Wall walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "PokemonBDSP_AutoStory_RouteSteps.h"
#include "PokemonBDSP_AutoStory_WallWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


WalkResult walk_to_wall_turning(RouteIO& io, std::optional<Pokemon::Direction> facing, const RouteWallWalk& walk){
    if (facing && *facing != walk.direction){
        turn(io, walk.direction);
    }
    WalkResult result = io.walk_to_wall(walk);
    if (!facing && result.stop == WalkStop::BLOCKED && result.moved_chunks > 0){
        io.note(std::format("[Route] {}: facing unknown; the first moved chunk may be the turn, not counted.", walk.label), RouteNote::LOG);
        result.moved_chunks--;
    }
    return result;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
