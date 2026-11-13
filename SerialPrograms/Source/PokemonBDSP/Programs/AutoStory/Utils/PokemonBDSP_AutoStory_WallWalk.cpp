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

using namespace std::chrono_literals;


namespace{

//  How long after a walk to a wall stopped to look for a dialogue.
constexpr Milliseconds FROZEN_DIALOG_WAIT = 1000ms;
constexpr size_t MAX_FROZEN_FIGHTS = 3;

WalkResult walk_fighting(RouteIO& io, RouteTrainers& trainers, std::optional<Pokemon::Direction> facing, const RouteWallWalk& walk){
    for (size_t fights = 0;; fights++){
        const WalkResult result = walk_to_wall_turning(io, facing, walk);
        if (fights == MAX_FROZEN_FIGHTS || result.stop != WalkStop::BLOCKED || !io.dialog_within(FROZEN_DIALOG_WAIT, COLOR_YELLOW)){
            return result;
        }
        io.note(std::format("[Route] {}: a trainer froze us short of the wall; battling.", walk.label), RouteNote::WARNING);
        if (!trainers.fight("a trainer at the wall")){
            return result;
        }
        facing.reset();
    }
}

} // namespace


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

WalkResult walk_on_to_wall(
    RouteIO& io, RouteTrainers& trainers,
    const std::optional<WallBefore>& before, bool began_still,
    const RouteWallWalk& walk
){
    io.note(std::format("[Route] {}: on to the wall after the battle.", walk.label), RouteNote::WARNING);
    std::optional<Pokemon::Direction> facing;
    if (before && began_still){
        io.note(std::format("[Route] {}: the battle began before this leg moved; on to the wall before it first.", walk.label), RouteNote::WARNING);
        RouteWallWalk back = walk;
        back.direction = before->direction;
        back.limit = before->limit;
        const WalkResult result = walk_fighting(io, trainers, std::nullopt, back);
        if (result.stop != WalkStop::BLOCKED && result.stop != WalkStop::LIMIT){
            fail(io, std::format("{}: {} on the walk on to the wall before it: not where the plan has us.", walk.label, walk_stop_name(result.stop)));
        }
        facing = before->direction;
    }
    return walk_fighting(io, trainers, facing, walk);
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
