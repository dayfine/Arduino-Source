/*  BDSP Wall Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <algorithm>
#include "PokemonBDSP_AutoStory_RobustRoute.h"
#include "PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP_AutoStory_WalkRules.h"
#include "PokemonBDSP_AutoStory_WallRecovery.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace{

using Pokemon::Direction;
using Pokemon::TilePos;

//  A wall ray must stay on the same map and walk one tile per step. A
//  ledge may be legal travel but must not become a localization anchor.
//  A wander box or a warp on the ray or its wall spoils it; a person who
//  never moves is a wall to stop against.
std::optional<WallHit> safe_wall(const StoryWorld& world, TilePos from, Direction direction){
    const auto map = world.to_local(from);
    if (!map || !standable(world, from) || recovery_hazard(world, from)){
        return std::nullopt;
    }
    const WallHit wall = slide_to_wall(world, from, direction);
    if (!wall.anchored || recovery_hazard(world, wall.wall)){
        return std::nullopt;
    }
    while (from != wall.stop){
        const auto next = step(world, from, direction);
        const auto local = next ? world.to_local(*next) : std::nullopt;
        if (!next || *next != from.next(direction) || !local || local->map != map->map
            || recovery_hazard(world, *next)){
            return std::nullopt;
        }
        from = *next;
    }
    return wall;
}

class WallPolicy{
public:
    using Hypothesis = TilePos;
    using Position = TilePos;
    using Hold = TileLeg;

    static Position position(const Hypothesis& at){ return at; }
    static int tiles(const Hold& hold){ return (int)hold.tiles; }
    static std::optional<Pokemon::WallRecoveryStep<Hypothesis, Hold>> hold(
        const StoryWorld& world, const std::vector<Hypothesis>& from, Direction direction
    ){
        std::vector<Hypothesis> ends;
        size_t longest = 0;
        for (const auto& start : from){
            const auto wall = safe_wall(world, start, direction);
            if (!wall){
                return std::nullopt;
            }
            longest = std::max(longest, wall->tiles);
            if (std::find(ends.begin(), ends.end(), wall->stop) == ends.end()){
                ends.push_back(wall->stop);
            }
        }
        return Pokemon::WallRecoveryStep<Hypothesis, Hold>{
            .positions = std::move(ends),
            .hold = {.direction = direction, .tiles = longest, .ends_at_wall = true, .held = false},
        };
    }

};

//  One D-pad tap, a whole tile for every possible position: safe when each
//  next tile is open ground on the same map with no one on it.
bool safe_tap(const StoryWorld& world, const TilePos& from, Direction direction){
    const auto map = world.to_local(from);
    const auto next = step(world, from, direction);
    const auto local = next ? world.to_local(*next) : std::nullopt;
    return map && next && *next == from.next(direction) && local && local->map == map->map && standable(world, *next)
        && !recovery_hazard(world, *next);
}

//  WallPolicy, with a tap where a hold has no wall for every position: a
//  tile with no safe wall any way (someone beside it, a door, the map's
//  edge) moves on a tile to where one is.
class WallOrTapPolicy : public WallPolicy{
public:
    static std::optional<Pokemon::WallRecoveryStep<Hypothesis, Hold>> hold(
        const StoryWorld& world, const std::vector<Hypothesis>& from, Direction direction
    ){
        std::optional<Pokemon::WallRecoveryStep<Hypothesis, Hold>> wall = WallPolicy::hold(world, from, direction);
        if (wall){
            return wall;
        }
        std::vector<Hypothesis> ends;
        for (const auto& start : from){
            if (!safe_tap(world, start, direction)){
                return std::nullopt;
            }
            ends.push_back(start.next(direction));
        }
        return Pokemon::WallRecoveryStep<Hypothesis, Hold>{
            .positions = std::move(ends),
            .hold = {.direction = direction, .tiles = 1, .ends_at_wall = false, .held = false},
        };
    }
};

} // namespace

std::vector<Pokemon::WallAnchor<Pokemon::TilePos, TileLeg>> wall_recovery_candidates(
    const StoryWorld& world, const std::vector<Pokemon::TilePos>& from,
    const Pokemon::WallRecoveryLimits& limits
){
    for (const auto& tile : from){
        if (!standable(world, tile)){
            return {};
        }
    }
    return Pokemon::find_wall_anchors<WallPolicy>(from, world, limits);
}

std::vector<TilePos> interrupted_leg_positions(
    const StoryWorld& world, const std::vector<TilePos>& from, Direction direction, size_t tiles
){
    std::vector<TilePos> positions;
    for (TilePos at : from){
        for (size_t count = 0; count <= tiles; count++){
            if (std::find(positions.begin(), positions.end(), at) == positions.end()){
                positions.push_back(at);
            }
            const auto next = step(world, at, direction);
            if (count == tiles || !next){
                break;
            }
            at = *next;
        }
    }
    return positions;
}

namespace{
//  The most holds, taps among them, plan_recovered_route() tries when walls
//  alone bring several tiles to no one wall.
constexpr size_t MAX_TAP_RECOVERY_HOLDS = 4;

std::vector<Pokemon::WallAnchor<TilePos, TileLeg>> tap_recovery_candidates(
    const StoryWorld& world, const std::vector<TilePos>& from
){
    for (const auto& tile : from){
        if (!standable(world, tile)){
            return {};
        }
    }
    return Pokemon::find_wall_anchors<WallOrTapPolicy>(from, world, {.max_holds = MAX_TAP_RECOVERY_HOLDS});
}

std::vector<PlannedLeg> recovery_legs(
    const StoryWorld& world, std::vector<TilePos> from, const std::vector<TileLeg>& holds
){
    std::vector<PlannedLeg> legs;
    for (const auto& hold : holds){
        std::vector<TilePos> after;
        for (const auto& at : from){
            const TilePos end = hold.ends_at_wall ? slide_to_wall(world, at, hold.direction).stop : at.next(hold.direction);
            if (std::find(after.begin(), after.end(), end) == after.end()){
                after.push_back(end);
            }
        }
        legs.push_back({.leg = hold, .after = after});
        from = std::move(after);
    }
    return legs;
}
} // namespace

std::optional<std::vector<PlannedLeg>> plan_recovered_route(
    const StoryWorld& world, const std::vector<TilePos>& from,
    const std::function<bool(const TilePos&)>& goal, const RobustPlanCosts& costs
){
    if (from.empty()){
        return std::nullopt;
    }
    if (std::all_of(from.begin(), from.end(), goal)){
        return std::vector<PlannedLeg>();
    }
    if (from.size() == 1){
        return plan_robust_route(world, from, goal, costs);
    }
    //  The cheapest whole walk, holds and the way on from the wall: the
    //  fewest held tiles alone chose "south 12 (to wall), north 12 (open)"
    //  over "north 13 (to wall), south 5 (open)" for 01.10's line 5 (lab-B
    //  2026-11-08 18:15:47). Ties keep find_wall_anchors()' order.
    //  Walls alone first; only when they bring the tiles to no one wall,
    //  D-pad taps where a hold has no wall, and a fourth hold. On lab-B
    //  (2026-11-12 02:52, 00.05c) a talk on Jubilife's row 24 left (35, 24)
    //  and (36, 24): from (36, 24) every way ends at no wall (the TV
    //  station's clown north, a man south, the woman's tile west, the map's
    //  edge east), and the walk failed with "no plan". Two taps east bring
    //  both tiles under the TV station's wall.
    std::optional<std::vector<PlannedLeg>> best;
    for (const bool taps : {false, true}){
        if (best){
            break;
        }
        long long best_cost = 0;
        const std::vector<Pokemon::WallAnchor<TilePos, TileLeg>> anchors = taps
            ? tap_recovery_candidates(world, from)
            : wall_recovery_candidates(world, from);
        for (const auto& anchor : anchors){
            const auto back = plan_robust_route(world, std::vector<TilePos>{anchor.at}, goal, costs);
            if (!back){
                continue;
            }
            auto route = recovery_legs(world, from, anchor.holds);
            route.insert(route.end(), back->begin(), back->end());
            const long long cost = robust_route_cost(world, from, route, costs);
            if (!best || cost < best_cost){
                best = std::move(route);
                best_cost = cost;
            }
        }
    }
    return best;
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
