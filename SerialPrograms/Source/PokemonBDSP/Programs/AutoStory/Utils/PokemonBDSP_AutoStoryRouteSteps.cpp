/*  BDSP AutoStory - Route steps helpers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_MoveMechanics.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryWalkRules.h"
#include "PokemonBDSP_AutoStoryRouteSteps.h"
#include "PokemonBDSP_AutoStoryRouteTiming.h"
#include "PokemonBDSP_AutoStoryWalkDpad.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace detail{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::TilePos;


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

std::vector<TilePos> sorted_unique_tiles(std::vector<TilePos> tiles){
    std::sort(tiles.begin(), tiles.end(), [](const TilePos& a, const TilePos& b){
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());
    return tiles;
}

std::vector<TilePos> wanderers_along_wall_leg(
    const StoryWorld& grid,
    const std::vector<TilePos>& from,
    Direction direction
){
    const Direction left = Direction(((int)direction + 1) % 4);
    const Direction right = Direction(((int)direction + 3) % 4);
    std::vector<TilePos> zone;
    for (const TilePos& pos : from){
        for (const TilePos& tile : leg_path(grid, pos, direction, SIZE_MAX)){
            for (const TilePos& near : {tile, tile.next(left), tile.next(right)}){
                if (wandering_hazard(grid, near)){
                    zone.push_back(near);
                }
            }
        }
    }
    return sorted_unique_tiles(std::move(zone));
}

std::vector<TilePos> tiles_before_wander_zone(
    const StoryWorld& grid,
    const std::vector<TilePos>& zone,
    Direction ahead
){
    const Direction back = Direction(((int)ahead + 2) % 4);
    std::vector<TilePos> before;
    for (const TilePos& tile : zone){
        if (const std::optional<TilePos> pos = step(grid, tile, back)){
            before.push_back(*pos);
        }
    }
    return sorted_unique_tiles(std::move(before));
}

namespace{

size_t max_safe_back_tiles(const StoryWorld& grid, const std::vector<TilePos>& stopped, Direction back){
    size_t most = 0;
    for (const TilePos& pos : stopped){
        const WallHit hit = slide_to_wall(grid, pos, back);
        const std::vector<TilePos> path = leg_path(grid, pos, back, hit.tiles);
        if (!hit.anchored || path.size() != hit.tiles || std::any_of(path.begin(), path.end(), [&](const TilePos& tile){
            return recovery_hazard(grid, tile);
        })){
            return 0;
        }
        most = std::max(most, hit.tiles);
    }
    return most;
}

BackToWallResult match_back_taps(
    const StoryWorld& grid, const std::vector<TilePos>& stopped,
    Direction back, size_t most, const WalkResult& taps, const std::string& label, RouteIO& io
){
    const size_t moved = std::min(most, taps.moved_chunks);
    std::vector<TilePos> found;
    std::vector<TilePos> now;
    std::vector<TilePos> shifted;
    for (const TilePos& pos : stopped){
        const WallHit hit = slide_to_wall(grid, pos, back);
        if (hit.tiles == moved && taps.wild_battles == 0){
            found.push_back(pos);
            now.push_back(hit.stop);
        }
        const std::vector<TilePos> path = leg_path(grid, pos, back, moved);
        if (path.size() == moved){
            shifted.push_back(moved == 0 ? pos : path.back());
        }
    }
    if (now.empty() && shifted.empty()){
        fail(io, std::format("{}: the taps back to the wall fit no tile.", label));
    }
    return {
        .tiles = sorted_unique_tiles(now.empty() ? std::move(shifted) : std::move(now)),
        .identified_stop = found.size() == 1 ? std::move(found) : std::vector<TilePos>{},
    };
}

} // namespace

BackToWallResult tap_back_to_wall(
    const StoryWorld& grid,
    const std::vector<TilePos>& stopped,
    Direction ahead,
    const std::string& label,
    RouteIO& io
){
    const Direction back = Direction(((int)ahead + 2) % 4);
    const size_t most = max_safe_back_tiles(grid, stopped, back);
    if (most == 0){
        return {};
    }
    io.note(std::format("[Route] {}: stopped on one of {} tiles; {} taps back to the wall to find which.",
        label, stopped.size(), most), RouteNote::WARNING);
    turn(io, back);
    const WalkResult taps = io.walk_taps(back, most, std::format("{}: back to the wall", label));
    if (taps.stop != WalkStop::BLOCKED && taps.stop != WalkStop::LIMIT){
        fail(io, std::format("{}: {} on the taps back to the wall.", label, walk_stop_name(taps.stop)));
    }
    return match_back_taps(grid, stopped, back, most, taps, label, io);
}


} // namespace detail
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
