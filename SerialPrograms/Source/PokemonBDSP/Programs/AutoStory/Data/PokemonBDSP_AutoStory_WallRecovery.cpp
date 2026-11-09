/*  BDSP Wall Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <algorithm>
#include "PokemonBDSP_AutoStory_WallRecovery.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace{

using Pokemon::Direction;
using Pokemon::TilePos;

//  A wall ray must stay on the same map and walk one tile per step. A
//  ledge may be legal travel but must not become a localization anchor.
std::optional<WallHit> safe_wall(const StoryWorld& world, TilePos from, Direction direction){
    const auto map = world.to_local(from);
    if (!map || !standable(world, from)
        || world.occupied(from, StoryWorld::OccupancyUse::WALL_RECOVERY) == StoryWorld::PERSON){
        return std::nullopt;
    }
    const WallHit wall = slide_to_wall(world, from, direction);
    if (!wall.anchored
        || world.occupied(wall.wall, StoryWorld::OccupancyUse::WALL_RECOVERY) == StoryWorld::PERSON){
        return std::nullopt;
    }
    while (from != wall.stop){
        const auto next = step(world, from, direction);
        const auto local = next ? world.to_local(*next) : std::nullopt;
        if (!next || *next != from.next(direction) || !local || local->map != map->map
            || world.occupied(*next, StoryWorld::OccupancyUse::WALL_RECOVERY) == StoryWorld::PERSON){
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
            .hold = {.direction = direction, .tiles = longest, .ends_at_wall = true},
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
std::vector<PlannedLeg> recovery_legs(
    const StoryWorld& world, std::vector<TilePos> from, const std::vector<TileLeg>& holds
){
    std::vector<PlannedLeg> legs;
    for (const auto& hold : holds){
        std::vector<TilePos> after;
        for (const auto& at : from){
            const TilePos end = slide_to_wall(world, at, hold.direction).stop;
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
    std::optional<std::vector<PlannedLeg>> best;
    long long best_cost = 0;
    for (const auto& anchor : wall_recovery_candidates(world, from)){
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
    return best;
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
