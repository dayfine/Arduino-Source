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
//  An open leg's extra per grass tile in a replan (RobustPlanCosts::
//  per_open_grass_tile): a wall leg through grass beats an open one unless
//  it walks this many tiles further. At 12, 01.06 up_jubilifes_road's
//  recovery from a battle in Route 203's first strip of grass tapped on
//  west through the second, and battles there chained more than 3
//  recoveries; at 60 the recovery leaves the grass on walls first.
constexpr int REPLAN_OPEN_GRASS_COST = 60;
//  Longest run into a trigger tried (trigger_runs()).
constexpr size_t TRIGGER_RUN_TILES = 32;
//  How much shorter or longer than the plan's open leg a hold into a
//  funnel is tried (add_funnel_holds()).
constexpr size_t FUNNEL_SLIDE = 2;
//  Shortest open leg run_variants() runs on or holds: shorter taps cost
//  little, and each variant is a plan to score on every step's first plan
//  (the BDSP sims plan thousands).
constexpr size_t RUN_VARIANT_TILES = 6;


//  Expected time a plan must save for each tile it walks back beyond the
//  route's own plan: the user's road route on Route 201 walks back 3
//  more and saves 24 s; 2 more to save 4.6 s (00.04b to_center_sign) or
//  10 more to save 10 s (01.05 east_of_barrys_trigger) is not worth the
//  change of route.
constexpr double WALK_BACK_WORTH_MS_PER_TILE = 3000;

//  Whether `score` is no worse than the route's own plan's in what the
//  golden bench checks besides the expected time: its worst time, its
//  places to be interrupted, its replans, and its tiles walked back
//  (Pokemon::tiles_walked_back()) unless each extra one saves
//  WALK_BACK_WORTH_MS_PER_TILE.
bool no_worse(const Pokemon::RouteScore& score, size_t back, const Pokemon::RouteScore& own, size_t own_back){
    if (score.worst_ms > own.worst_ms || score.recoveries > own.recoveries || score.replans > own.replans){
        return false;
    }
    return back <= own_back || own.expected_ms - score.expected_ms >= WALK_BACK_WORTH_MS_PER_TILE * (double)(back - own_back);
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

Direction opposite(Direction direction){
    return Direction(((int)direction + 2) % 4);
}

//  Where every tile of `from` stops walking `direction` to a wall, or
//  nothing unless each is held at one (WallHit::anchored).
std::optional<PlannedLeg> wall_leg_from(const StoryWorld& world, const std::vector<TilePos>& from, Direction direction){
    PlannedLeg out{.leg = {.direction = direction, .tiles = 0, .ends_at_wall = true, .held = false}, .after = {}};
    for (const TilePos& pos : from){
        const WallHit hit = slide_to_wall(world, pos, direction);
        if (!hit.anchored || hit.tiles == 0){
            return std::nullopt;
        }
        out.leg.tiles = std::max(out.leg.tiles, hit.tiles);
        if (std::find(out.after.begin(), out.after.end(), hit.stop) == out.after.end()){
            out.after.push_back(hit.stop);
        }
    }
    return out;
}

//  `legs` with its open leg `index`, from the one tile `from`, run on to
//  the wall past its end and tapped back: 00.04 into_twinleaf, "south 33"
//  tapped (26.5 s) or run south 37 to the wall and 4 taps north (10.2 s).
std::optional<std::vector<PlannedLeg>> overshoot_to_wall(
    const StoryWorld& world, const TilePos& from, const std::vector<PlannedLeg>& legs, size_t index
){
    const PlannedLeg& open = legs[index];
    const std::optional<PlannedLeg> wall = wall_leg_from(world, {from}, open.leg.direction);
    if (!wall || open.after.size() != 1 || wall->leg.tiles <= open.leg.tiles){
        return std::nullopt;
    }
    const TilePos end = open.after.front();
    const TilePos stop = wall->after.front();
    const size_t back = (size_t)(std::abs(stop.x - end.x) + std::abs(stop.y - end.y));
    if (stop.x != end.x && stop.y != end.y){
        return std::nullopt;
    }
    std::vector<PlannedLeg> out(legs.begin(), legs.begin() + index);
    out.push_back(*wall);
    out.push_back({.leg = {.direction = opposite(open.leg.direction), .tiles = back, .ends_at_wall = false, .held = false}, .after = {end}});
    out.insert(out.end(), legs.begin() + index + 1, legs.end());
    return out;
}

//  `legs` up to its open leg `index`, that leg held for its time from the
//  one tile `from` (it may end a tile short or long), then a run to the
//  wall across it that stops every end, and a plan on from there (walls
//  that bring the ends to one tile first): 00.04 along_route_201_road, run
//  west along the road into the funnel whose walls stop a run north.
std::optional<std::vector<PlannedLeg>> hold_into_funnel(
    const ScoredRoute& route, const TilePos& from, const std::vector<PlannedLeg>& legs, size_t index, size_t tiles, Direction across
){
    const TileLeg& open = legs[index].leg;
    const size_t error = open_hold_error(tiles);
    std::vector<TilePos> ends;
    TilePos at = from;
    for (size_t n = 1; n <= tiles + error; n++){
        const std::optional<TilePos> next = step(route.world, at, open.direction);
        if (!next || std::abs(next->x - at.x) + std::abs(next->y - at.y) != 1){
            return std::nullopt;
        }
        at = *next;
        if (n + error >= tiles){
            ends.push_back(at);
        }
    }
    const std::optional<PlannedLeg> wall = wall_leg_from(route.world, ends, across);
    if (!wall){
        return std::nullopt;
    }
    const std::optional<std::vector<PlannedLeg>> rest = plan_recovered_route(route.world, wall->after, route.goal, route.costs);
    if (!rest){
        return std::nullopt;
    }
    std::vector<PlannedLeg> out(legs.begin(), legs.begin() + index);
    out.push_back({.leg = {.direction = open.direction, .tiles = tiles, .ends_at_wall = false, .held = true}, .after = ends});
    out.push_back(*wall);
    out.insert(out.end(), rest->begin(), rest->end());
    return out;
}

//  hold_into_funnel() for open leg `index` held a little shorter or longer
//  than the plan has it, to end where a funnel catches every end: 00.04's
//  road plan taps 16 west, the funnel is 17.
void add_funnel_holds(std::vector<std::vector<PlannedLeg>>& out,
    const ScoredRoute& route, const TilePos& from, const std::vector<PlannedLeg>& legs, size_t index
){
    const TileLeg& leg = legs[index].leg;
    for (size_t tiles = leg.tiles > FUNNEL_SLIDE + 2 ? leg.tiles - FUNNEL_SLIDE : 2; tiles <= leg.tiles + FUNNEL_SLIDE; tiles++){
        for (Direction across : {Direction((int(leg.direction) + 1) % 4), Direction((int(leg.direction) + 3) % 4)}){
            if (std::optional<std::vector<PlannedLeg>> legs_out = hold_into_funnel(route, from, legs, index, tiles, across)){
                out.push_back(std::move(*legs_out));
            }
        }
    }
}

//  Plans made from `legs` the way a player runs: each open leg from a
//  known tile run on to a wall and tapped back, or held into a wall that
//  re-localizes it. The search over sets of tiles (RobustSearch) seldom
//  finds these within its budget once holds are allowed.
std::vector<std::vector<PlannedLeg>> run_variants(const ScoredRoute& route, const std::vector<PlannedLeg>& legs){
    std::vector<std::vector<PlannedLeg>> out;
    std::vector<TilePos> at = route.from;
    for (size_t i = 0; i < legs.size(); i++){
        const TileLeg& leg = legs[i].leg;
        if (!leg.ends_at_wall && !leg.held && leg.tiles >= RUN_VARIANT_TILES && at.size() == 1){
            if (std::optional<std::vector<PlannedLeg>> legs_out = overshoot_to_wall(route.world, at.front(), legs, i)){
                out.push_back(std::move(*legs_out));
            }
            add_funnel_holds(out, route, at.front(), legs, i);
        }
        at = legs[i].after;
    }
    return out;
}

bool same_legs(const std::vector<PlannedLeg>& a, const std::vector<PlannedLeg>& b){
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](const PlannedLeg& x, const PlannedLeg& y){
        return x.leg.direction == y.leg.direction && x.leg.tiles == y.leg.tiles &&
            x.leg.ends_at_wall == y.leg.ends_at_wall && x.leg.held == y.leg.held;
    });
}

//  A run `direction` from `from` that a goal tile on its way stops, the
//  goal a trigger: nothing if a wall or the maps' edge comes first.
std::optional<PlannedLeg> run_into_goal(const ScoredRoute& route, const TilePos& from, Direction direction){
    TilePos at = from;
    for (size_t tiles = 1; tiles <= TRIGGER_RUN_TILES; tiles++){
        const std::optional<TilePos> next = step(route.world, at, direction);
        if (!next || std::abs(next->x - at.x) + std::abs(next->y - at.y) != 1){
            return std::nullopt;
        }
        at = *next;
        if (route.goal(at)){
            return PlannedLeg{.leg = {.direction = direction, .tiles = tiles, .ends_at_wall = true, .held = false}, .after = {at}};
        }
    }
    return std::nullopt;
}

//  To a trigger (ScoredRoute::goal_is_trigger): `legs` up to a leg from a
//  known tile, then a run the trigger stops. 00.04 into_moms_call taps
//  west 4 onto Mom's call on (6, 9), or runs west until it stops us there.
std::vector<std::vector<PlannedLeg>> trigger_runs(const ScoredRoute& route, const std::vector<PlannedLeg>& legs){
    std::vector<std::vector<PlannedLeg>> out;
    if (!route.goal_is_trigger){
        return out;
    }
    std::vector<TilePos> at = route.from;
    for (size_t i = 0; i < legs.size(); i++){
        if (at.size() == 1){
            for (Direction direction : {Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST}){
                if (std::optional<PlannedLeg> run = run_into_goal(route, at.front(), direction)){
                    std::vector<PlannedLeg> legs_out(legs.begin(), legs.begin() + i);
                    legs_out.push_back(std::move(*run));
                    out.push_back(std::move(legs_out));
                }
            }
        }
        at = legs[i].after;
    }
    return out;
}

//  The scorer's view of `route`'s world: a trigger goal stops a walk.
Pokemon::ScoreMap score_map(const ScoredRoute& route){
    Pokemon::ScoreMap map = route_score_map(route.world, route.triggers);
    if (route.goal_is_trigger){
        map.risk = [risk = std::move(map.risk), goal = route.goal](const TilePos& pos){
            return goal(pos) ? Pokemon::TileRisk::TRIGGER : risk(pos);
        };
    }
    return map;
}

Pokemon::RouteScore score_with(const ScoredRoute& route, const std::vector<PlannedLeg>& legs, const Pokemon::RouteReplanner& replan){
    return Pokemon::score_route(score_map(route), route.from, route.goal,
        route_scored_legs(legs, route.speed), route_score_paces(), route_interrupt_model(), replan);
}

//  The other plans plan_fastest_route() scores besides `own`, the plan
//  with the route's costs.
std::vector<std::vector<PlannedLeg>> candidate_plans(const ScoredRoute& route, const std::vector<PlannedLeg>& own){
    std::vector<std::vector<PlannedLeg>> out = run_variants(route, own);
    for (std::vector<PlannedLeg>& legs : trigger_runs(route, own)){
        out.push_back(std::move(legs));
    }
    std::optional<std::vector<PlannedLeg>> grass = plan_recovered_route(route.world, route.from, route.goal, replan_costs(route.costs));
    if (grass && !same_legs(*grass, own)){
        out.push_back(std::move(*grass));
    }
    return out;
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
    std::vector<std::vector<PlannedLeg>> candidates = candidate_plans(route, *best);
    //  Most steps have no other plan: nothing to score.
    if (candidates.empty()){
        return best;
    }
    CachedReplanner replanner(route);
    const Pokemon::RouteReplanner replan = replanner.replanner();
    const Pokemon::RouteScore own = score_with(route, *best, replan);
    const bool own_passes = own.verdict == Pokemon::RouteVerdict::PASS;
    const size_t own_back = Pokemon::tiles_walked_back(route_scored_legs(*best, route.speed));
    double best_ms = own_passes ? own.expected_ms : -1;
    for (std::vector<PlannedLeg>& legs : candidates){
        const Pokemon::RouteScore score = score_with(route, legs, replan);
        if (score.verdict != Pokemon::RouteVerdict::PASS || (best_ms >= 0 && score.expected_ms >= best_ms)){
            continue;
        }
        if (own_passes && !no_worse(score, Pokemon::tiles_walked_back(route_scored_legs(legs, route.speed)), own, own_back)){
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
