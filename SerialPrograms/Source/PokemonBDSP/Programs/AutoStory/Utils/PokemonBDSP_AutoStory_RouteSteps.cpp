/*  BDSP AutoStory - Route steps helpers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/Navigation/Pokemon_MoveMechanics.h"
#include "PokemonBDSP_AutoStory_RouteSteps.h"
#include "PokemonBDSP_AutoStory_RouteTiming.h"
#include "PokemonBDSP_AutoStory_WalkDpad.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace detail{

using namespace std::chrono_literals;
using Pokemon::Direction;


Milliseconds wall_limit(const TileLeg& leg){
    const Milliseconds per_tile = route_ms_per_tile(leg.direction, MoveSpeed::WALK);
    return ROUTE_START_DELAY + per_tile * leg.tiles * 5 / 4 + 1000ms;
}

[[noreturn]] void fail(RouteIO& io, const std::string& message){
    io.note(message, RouteNote::FAILURE);
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "RouteIO::note() returned from a failure: " + message);
}

void turn(RouteIO& io, Direction direction){
    io.note(std::string("[Route] Turning ") + direction_name(direction) + ".", RouteNote::LOG);
    io.press(route_turn_tap(direction));
}

RouteInput press_a(RouteInputEnd end){
    return {.kind = RouteInputKind::BUTTON_A, .direction = Direction::NORTH, .hold = 80ms, .release = 200ms,
        .count = 1, .end = end};
}

size_t tiles_moved(const WalkResult& walk, Milliseconds per_tile){
    if (walk.stop == WalkStop::LIMIT && walk.moved_chunks > 0){
        const Milliseconds moving = walk.walked - ROUTE_START_DELAY;
        const size_t tiles_from_time = moving.count() <= 0 ? (size_t)0 : (size_t)(moving / per_tile);
        const size_t tiles_from_chunks = (walk.moved_chunks * 400ms) / per_tile;
        return std::min(tiles_from_time, std::max(walk.moved_chunks, tiles_from_chunks));
    }
    const Milliseconds moving = walk.walked - ROUTE_START_DELAY -
        (walk.stop == WalkStop::BLOCKED ? 800ms : 0ms);
    return moving.count() <= 0 ? (size_t)0 : (size_t)(moving / per_tile);
}


} // namespace detail
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
