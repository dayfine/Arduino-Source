/*  BDSP AutoStory - Route Choice
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "PokemonBDSP_AutoStory_RouteChoice.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::ScoredLeg;
using Pokemon::ScoredLegKind;
using Pokemon::TilePos;


namespace{

//  A tap of walk_tiles_dpad(): it allows 800 ms a tile, and the console
//  taps took about that (00.04's "west 16 (open)" took 12.8 s).
constexpr long long TAP_MS = 800;
//  Confirming a stop at a wall: walk_until_dpad() watches a still screen.
constexpr long long WALL_STOP_MS = 1000;
//  Most sets a search with held legs looks through: their wide ends make
//  many more sets than taps do, and walk_route() plans again after every
//  interruption, so a held plan not found quickly is left to the taps.
constexpr size_t HELD_MAX_SEARCHED = 3000;

//  The costs plan_fastest_route() tries besides the route's own: open legs
//  held from 2 tiles, as long as a held leg may be (a column of 33
//  tiles into Twinleaf), with and without the route's charge for ending in
//  the open.
std::vector<RobustPlanCosts> held_costs(const RobustPlanCosts& costs){
    std::vector<RobustPlanCosts> out;
    for (int exact : {1}){
        for (int not_at_wall : {costs.leg_not_at_wall, 0}){
            RobustPlanCosts held = costs;
            held.hold_open_legs = true;
            held.exact_open_tiles = exact;
            held.max_open_tiles = 36;
            held.leg_not_at_wall = not_at_wall;
            held.max_searched = std::min<size_t>(costs.max_searched, HELD_MAX_SEARCHED);
            out.push_back(held);
        }
    }
    return out;
}

//  Whether `score` beats `best`, the route's own plan's (if it passed): it
//  passes in less time, and where `best` passed, with no more places to be
//  interrupted and no more stops or replans to re-localize.
bool faster(const Pokemon::RouteScore& score, const std::optional<Pokemon::RouteScore>& own, double best_ms){
    if (score.verdict != Pokemon::RouteVerdict::PASS || (best_ms >= 0 && score.expected_ms >= best_ms)){
        return false;
    }
    return !own || (score.recoveries <= own->recoveries && score.stops + score.replans <= own->stops + own->replans);
}

} // namespace


Pokemon::ScoreMap route_score_map(const StoryWorld& world, std::vector<TilePos> triggers){
    return {
        .step = [&world](const TilePos& from, Direction direction) -> std::optional<TilePos>{
            const std::optional<TilePos> next = step(world, from, direction);
            if (!next && world.tile(from.next(direction)) == Pokemon::TileKind::DOOR){
                return from.next(direction);
            }
            return next;
        },
        .risk = [&world, triggers = std::move(triggers)](const TilePos& pos){
            if (std::find(triggers.begin(), triggers.end(), pos) != triggers.end()){
                return Pokemon::TileRisk::TRIGGER;
            }
            const std::optional<Pokemon::TileKind> tile = world.tile(pos);
            if (tile == Pokemon::TileKind::DOOR){
                return Pokemon::TileRisk::WARP;
            }
            if (tile == Pokemon::TileKind::TALL_GRASS){
                return Pokemon::TileRisk::ENCOUNTER;
            }
            return wandering_hazard(world, pos) ? Pokemon::TileRisk::WANDERER : Pokemon::TileRisk::NONE;
        },
    };
}

Pokemon::RoutePaces route_score_paces(){
    return {
        .ms_per_tile = [](Direction direction, bool run){
            return (long long)route_ms_per_tile(direction, run ? MoveSpeed::RUN : MoveSpeed::WALK).count();
        },
        .start_ms = ROUTE_START_DELAY.count(),
        .tap_ms = TAP_MS,
        .press_ms = ROUTE_TURN_TAP.count() * 2,
        .stop_ms = WALL_STOP_MS,
    };
}

std::vector<ScoredLeg> route_scored_legs(const std::vector<PlannedLeg>& legs, MoveSpeed speed){
    std::vector<ScoredLeg> out;
    for (const PlannedLeg& planned : legs){
        const TileLeg& leg = planned.leg;
        const ScoredLegKind kind = leg.ends_at_wall ? ScoredLegKind::TO_WALL : leg.held ? ScoredLegKind::OPEN_HOLD : ScoredLegKind::TAPS;
        out.push_back({.direction = leg.direction, .tiles = leg.tiles, .kind = kind, .run = speed == MoveSpeed::RUN});
    }
    return out;
}

Pokemon::RouteScore score_planned_route(const ScoredRoute& route, const std::vector<PlannedLeg>& legs){
    const Pokemon::RouteReplanner replan = [&route](const std::vector<TilePos>& from) -> std::optional<std::vector<ScoredLeg>>{
        const std::optional<std::vector<PlannedLeg>> again = plan_recovered_route(route.world, from, route.goal, route.costs);
        if (!again){
            return std::nullopt;
        }
        return route_scored_legs(*again, route.speed);
    };
    return Pokemon::score_route(route_score_map(route.world, route.triggers), route.from, route.goal,
        route_scored_legs(legs, route.speed), route_score_paces(), Pokemon::InterruptModel(), replan);
}

std::vector<TilePos> blocked_stop_tiles(const StoryWorld& world, const std::vector<TilePos>& stopped, Direction direction){
    std::vector<TilePos> out;
    const Direction back = Direction(((int)direction + 2) % 4);
    for (const TilePos& pos : stopped){
        out.push_back(pos);
        for (Direction way : {back, direction}){
            const std::optional<TilePos> next = step(world, pos, way);
            if (next && std::abs(next->x - pos.x) + std::abs(next->y - pos.y) == 1){
                out.push_back(*next);
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const TilePos& a, const TilePos& b){
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::optional<std::vector<PlannedLeg>> plan_fastest_route(const ScoredRoute& route){
    std::optional<std::vector<PlannedLeg>> best = plan_recovered_route(route.world, route.from, route.goal, route.costs);
    if (!best){
        return best;
    }
    const Pokemon::RouteScore first = score_planned_route(route, *best);
    const std::optional<Pokemon::RouteScore> own = first.verdict == Pokemon::RouteVerdict::PASS
        ? std::optional<Pokemon::RouteScore>(first) : std::nullopt;
    double best_ms = own ? first.expected_ms : -1;
    for (const RobustPlanCosts& costs : held_costs(route.costs)){
        std::optional<std::vector<PlannedLeg>> legs = plan_recovered_route(route.world, route.from, route.goal, costs);
        if (!legs){
            continue;
        }
        const Pokemon::RouteScore score = score_planned_route(route, *legs);
        if (faster(score, own, best_ms)){
            best = std::move(legs);
            best_ms = score.expected_ms;
        }
    }
    return best;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
