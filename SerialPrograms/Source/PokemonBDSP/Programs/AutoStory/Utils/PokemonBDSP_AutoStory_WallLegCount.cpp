/*  BDSP AutoStory - Wall leg count
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "PokemonBDSP_AutoStory_RouteTiming.h"
#include "PokemonBDSP_AutoStory_WallLegCount.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


//  Legs to a wall this many tiles or shorter must walk every tile: a tile
//  short is someone in the way, not the time count's slack. On lab-B
//  (2026-11-08, 01.08) Route 204's youngster stopped "west 2 (to wall)"
//  after 1 tile; a tile short passed as at the wall, the route logged
//  arriving at (11, 2) from (13, 2), and the taps north hit the cliff.
constexpr size_t EXACT_WALL_LEG_TILES = 3;

//  Tiles a walk to the wall moved, from the time it walked.
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

//  Tiles a walk on to the wall after a stop moved: none if no chunk moved.
//  A push against someone still in the way reads as stopped late, and its
//  time counted as tiles put a walk stopped 3 tiles in at its wall 17 tiles
//  away (the sim of lab-B 2026-11-12 02:52).
size_t moved_on(const WalkResult& walk, Milliseconds per_tile){
    return walk.moved_chunks == 0 ? 0 : tiles_moved(walk, per_tile);
}

//  Whether a walk to the wall never moved: no chunk changed and its time
//  counts no tile. It stopped where it began, so whoever stopped it stands
//  on the next tile, however short the leg. On lab-B (2026-11-08 16:50,
//  01.08) Route 204's wandering youngster stood a tile ahead of "west 2 (to
//  wall)" from (13, 2), and a replan from (12, 2) and (13, 2) found no plan:
//  no wall is anchored from a tile someone wanders on.
bool never_moved(const WalkResult& walk, size_t walked_tiles){
    return walk.stop == WalkStop::BLOCKED && walk.moved_chunks == 0 && walked_tiles == 0;
}

//  Whether a walk to the wall that stopped took far longer than the tiles
//  to its nearest wall `nearest` allow, past the walker's slack: two of its
//  400 ms chunks, or 30% of the tiles' time on a long leg. Something held
//  it up on the way, and its time no longer counts tiles. On lab-B (2026-11-11 21:00, 00.05b) "east 8 (to wall)" stopped after 2800 ms
//  against a boy below the Mart, about 5 tiles in, but the time counted 17.
bool held_up(const WalkResult& walk, size_t nearest, Milliseconds per_tile){
    if (walk.stop != WalkStop::BLOCKED || walk.wild_battles > 0){
        return false;
    }
    const Milliseconds tiles_time = per_tile * nearest;
    const Milliseconds slack = std::max<Milliseconds>(800ms, tiles_time * 3 / 10);
    //  800 ms: the two still chunks that read as stopped (tiles_moved()).
    return walk.walked > ROUTE_START_DELAY + tiles_time + 800ms + slack;
}

//  Legs to a wall this many tiles or shorter must walk every tile: a tile
//  short is someone in the way, not the time count's slack. On lab-B
//  (2026-11-08, 01.08) Route 204's youngster stopped "west 2 (to wall)"
//  after 1 tile; a tile short passed as at the wall, the route logged
//  arriving at (11, 2) from (13, 2), and the taps north hit the cliff.

//  The fewest tiles a leg to a wall `expected_tiles` away must have walked
//  to count as at its wall.
size_t min_tiles_at_wall(size_t expected_tiles){
    return expected_tiles > EXACT_WALL_LEG_TILES ? expected_tiles * 7 / 10 : expected_tiles;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
