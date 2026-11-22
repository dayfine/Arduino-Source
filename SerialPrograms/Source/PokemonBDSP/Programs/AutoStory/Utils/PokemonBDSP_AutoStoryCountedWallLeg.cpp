/*  BDSP AutoStory - Counted wall leg
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "PokemonBDSP_AutoStoryCountedWallLeg.h"
#include "PokemonBDSP_AutoStoryRouteSteps.h"
#include "PokemonBDSP_AutoStoryWalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  Waits for the way at most this often in one counted leg: Oreburgh's
//  Hiker tries a step every 2 s, and may stay or keep to the column.
constexpr size_t COUNTED_WALL_WAITS = 8;

//  A count by time is off by a tile or so a stop. Short of the wall by more
//  than WAIT_SHORT_OF is someone in the way (Oreburgh's Hiker stops us 4
//  to 7 tiles short of the Center's wall); past COUNT_SLACK either way at
//  the end, the walk is not where the step has us.
constexpr size_t COUNTED_WALL_WAIT_SHORT_OF = 2;
constexpr size_t COUNTED_WALL_COUNT_SLACK = 3;

//  One hold of `leg` to the wall; fails on anything but a stop or the time.
WalkResult hold_counted_wall_leg(RouteIO& io, const CountedWallLeg& leg){
    const WalkResult result = io.walk_to_wall({.direction = leg.direction,
        .limit = detail::wall_limit({.direction = leg.direction, .tiles = leg.most}), .label = leg.label,
        .options = {.arrived = nullptr, .pace = DpadPace::RUN}});
    if (result.stop != WalkStop::BLOCKED && result.stop != WalkStop::LIMIT){
        detail::fail(io, leg.label + ": " + walk_stop_name(result.stop) + " instead of a stop.");
    }
    if (result.wild_battles > 0){
        detail::fail(io, leg.label + ": a wild battle on the way; the count of tiles is lost.");
    }
    return result;
}

} // namespace

size_t walk_counted_wall_leg(RouteIO& io, const CountedWallLeg& leg){
    const Milliseconds per_tile = route_ms_per_tile(leg.direction, leg.speed);
    const size_t slack = COUNTED_WALL_COUNT_SLACK;
    size_t walked = 0;
    for (size_t waits = 0;; waits++){
        const WalkResult result = hold_counted_wall_leg(io, leg);
        walked += detail::tiles_moved(result, per_tile);
        if (result.stop == WalkStop::LIMIT){
            //  The stop went unseen: we must be held against the wall.
            if (!io.holding_against_wall(leg.direction, leg.label + ": at the wall?")){
                detail::fail(io, leg.label + ": ran out of time after about " + std::to_string(walked) + " tiles, short of the wall.");
            }
            break;
        }
        if (walked + COUNTED_WALL_WAIT_SHORT_OF >= leg.fewest || waits == COUNTED_WALL_WAITS){
            break;
        }
        io.note("[Route] " + leg.label + ": stopped after about " + std::to_string(walked) +
            " tiles, short of the wall; waiting for the way to clear.", RouteNote::WARNING);
        io.press(detail::WAIT_FOR_THE_WAY);
    }
    io.note(std::format("[Route] {}: about {} tiles to the wall; the map has {} to {}.", leg.label, walked, leg.fewest, leg.most),
        RouteNote::LOG);
    if (walked + slack < leg.fewest || walked > leg.most + slack){
        detail::fail(io, std::format("{}: about {} tiles to the wall, where the map has {} to {}: not where the step has us.",
            leg.label, walked, leg.fewest, leg.most));
    }
    return walked;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
