/*  BDSP AutoStory - Route Choice
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <map>
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
//  Interruptions, estimated (not measured): a wild battle a tile in tall
//  grass one time in ten, and run from in 12 s; a wanderer in the way a
//  tile in four of their zone, waited out in 2.5 s; a trainer battle
//  45 s; a stop and a new plan 2 s.
constexpr double ENCOUNTER_PER_TILE = 0.1;
constexpr long long WILD_BATTLE_MS = 12000;
constexpr double WANDERER_PER_TILE = 0.25;
constexpr long long WANDERER_WAIT_MS = 2500;
constexpr long long TRAINER_BATTLE_MS = 45000;
constexpr long long REPLAN_MS = 2000;
//  Most sets a search with held legs looks through: their wide ends make
//  many more sets than taps do, so a held plan not found quickly is left
//  to the taps.
constexpr size_t HELD_MAX_SEARCHED = 3000;
//  A tapped tile costs this many run tiles in a plan's search: a tap takes
//  TAP_MS, a run about 250 ms a tile.
constexpr int TAP_TILE_COST = 3;
//  Taps this many tiles or fewer in the time-shaped costs: tapping back
//  from a wall onto the goal.
constexpr int TAPPED_BACK_TILES = 8;
//  An open leg's extra per grass tile in a replan (RobustPlanCosts::
//  per_open_grass_tile): a wall leg through grass beats an open one unless
//  it walks this many tiles further.
constexpr int REPLAN_OPEN_GRASS_COST = 12;

//  The costs plan_fastest_route() tries besides the route's own:
//   -  open legs held from 2 tiles, as long as a held leg may be (a column
//      of 33 tiles into Twinleaf), with and without the route's charge for
//      ending in the open;
//   -  costs shaped like the time: a tapped tile dearer than a run one,
//      no charge for ending in the open, so a run to a wall past the goal
//      and taps back beat taps all the way (00.04 into_twinleaf);
//   -  the route's own, with open legs dear in grass (replan_costs()).
std::vector<RobustPlanCosts> other_costs(const RobustPlanCosts& costs){
    std::vector<RobustPlanCosts> out;
    for (int not_at_wall : {costs.leg_not_at_wall, 0}){
        RobustPlanCosts held = costs;
        held.hold_open_legs = true;
        held.exact_open_tiles = 1;
        held.max_open_tiles = 36;
        held.leg_not_at_wall = not_at_wall;
        held.max_searched = std::min<size_t>(costs.max_searched, HELD_MAX_SEARCHED);
        out.push_back(held);
    }
    RobustPlanCosts timed = costs;
    timed.hold_open_legs = true;
    timed.exact_open_tiles = TAPPED_BACK_TILES;
    timed.max_open_tiles = 36;
    timed.per_open_tile = TAP_TILE_COST;
    timed.leg_not_at_wall = 0;
    timed.max_searched = std::min<size_t>(costs.max_searched, HELD_MAX_SEARCHED);
    out.push_back(timed);
    out.push_back(replan_costs(costs));
    return out;
}

//  Expected time a plan must save to walk back more than the route's own
//  plan: a run past the goal to a wall and taps back saved 16 s into
//  Twinleaf, the user's own route there; a second or two elsewhere is not
//  worth the change of route.
constexpr double WALK_BACK_WORTH_MS = 5000;

//  Whether `score` is no worse than the route's own plan's in what the
//  golden bench checks besides the expected time: its worst time, its
//  places to be interrupted, its replans, and its backtracking (replans
//  and tiles walked back), unless it saves WALK_BACK_WORTH_MS.
bool no_worse(const Pokemon::RouteScore& score, size_t back, const Pokemon::RouteScore& own, size_t own_back){
    if (score.worst_ms > own.worst_ms || score.recoveries > own.recoveries || score.replans > own.replans){
        return false;
    }
    return score.replans + back <= own.replans + own_back || own.expected_ms - score.expected_ms >= WALK_BACK_WORTH_MS;
}

//  A replanner for the scorer that plans each set of tiles once, for every
//  plan scored with it.
class CachedReplanner{
public:
    explicit CachedReplanner(const ScoredRoute& route) : m_route(route) {}
    Pokemon::RouteReplanner replanner(){
        return [this](const std::vector<TilePos>& from) -> std::optional<std::vector<ScoredLeg>>{
            std::vector<TilePos> key = from;
            std::sort(key.begin(), key.end(), [](const TilePos& a, const TilePos& b){
                return a.y != b.y ? a.y < b.y : a.x < b.x;
            });
            const auto found = m_plans.find(key);
            if (found != m_plans.end()){
                return found->second;
            }
            std::optional<std::vector<ScoredLeg>> legs;
            const std::optional<std::vector<PlannedLeg>> again =
                plan_recovered_route(m_route.world, from, m_route.goal, replan_costs(m_route.costs));
            if (again){
                legs = route_scored_legs(*again, m_route.speed);
            }
            m_plans.emplace(std::move(key), legs);
            return legs;
        };
    }

private:
    struct TileLess{
        bool operator()(const std::vector<TilePos>& a, const std::vector<TilePos>& b) const{
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](const TilePos& x, const TilePos& y){
                return x.y != y.y ? x.y < y.y : x.x < y.x;
            });
        }
    };
    const ScoredRoute& m_route;
    std::map<std::vector<TilePos>, std::optional<std::vector<ScoredLeg>>, TileLess> m_plans;
};

Pokemon::RouteScore score_with(const ScoredRoute& route, const std::vector<PlannedLeg>& legs, const Pokemon::RouteReplanner& replan){
    return Pokemon::score_route(route_score_map(route.world, route.triggers), route.from, route.goal,
        route_scored_legs(legs, route.speed), route_score_paces(), route_interrupt_model(), replan);
}

} // namespace


RobustPlanCosts replan_costs(const RobustPlanCosts& costs){
    RobustPlanCosts out = costs;
    out.per_open_grass_tile = std::max(out.per_open_grass_tile, REPLAN_OPEN_GRASS_COST);
    return out;
}


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
        .open_hold_error = open_hold_error,
    };
}

Pokemon::InterruptModel route_interrupt_model(){
    Pokemon::InterruptModel model;
    model.encounter_per_tile = ENCOUNTER_PER_TILE;
    model.encounter_ms = WILD_BATTLE_MS;
    model.wanderer_per_tile = WANDERER_PER_TILE;
    model.wanderer_ms = WANDERER_WAIT_MS;
    model.trainer_ms = TRAINER_BATTLE_MS;
    model.replan_ms = REPLAN_MS;
    return model;
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
    CachedReplanner replanner(route);
    return score_with(route, legs, replanner.replanner());
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
    CachedReplanner replanner(route);
    const Pokemon::RouteReplanner replan = replanner.replanner();
    const Pokemon::RouteScore own = score_with(route, *best, replan);
    const bool own_passes = own.verdict == Pokemon::RouteVerdict::PASS;
    const size_t own_back = Pokemon::tiles_walked_back(route_scored_legs(*best, route.speed));
    double best_ms = own_passes ? own.expected_ms : -1;
    for (const RobustPlanCosts& costs : other_costs(route.costs)){
        std::optional<std::vector<PlannedLeg>> legs = plan_recovered_route(route.world, route.from, route.goal, costs);
        if (!legs){
            continue;
        }
        const Pokemon::RouteScore score = score_with(route, *legs, replan);
        if (score.verdict != Pokemon::RouteVerdict::PASS || (best_ms >= 0 && score.expected_ms >= best_ms)){
            continue;
        }
        if (own_passes && !no_worse(score, Pokemon::tiles_walked_back(route_scored_legs(*legs, route.speed)), own, own_back)){
            continue;
        }
        best = std::move(legs);
        best_ms = score.expected_ms;
    }
    return best;
}

std::optional<std::vector<PlannedLeg>> plan_walk(const ScoredRoute& route, bool first){
    return first
        ? plan_fastest_route(route)
        : plan_recovered_route(route.world, route.from, route.goal, replan_costs(route.costs));
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
