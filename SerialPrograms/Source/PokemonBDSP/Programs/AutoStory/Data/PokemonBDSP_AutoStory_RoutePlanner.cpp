/*  BDSP AutoStory - Route planner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/Navigation/Pokemon_LegPlanner.h"
#include "Pokemon/Navigation/Pokemon_RobustLegPlanner.h"
#include "PokemonBDSP_AutoStory_RobustRoute.h"
#include "PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP_AutoStory_TrainerSight.h"
#include "PokemonBDSP_AutoStory_WalkRules.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::TileKind;
using Pokemon::TilePos;


namespace{

size_t distance(const TilePos& a, const TilePos& b){
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

bool tall_grass(const StoryWorld& world, const TilePos& pos){
    return world.tile(pos) == TileKind::TALL_GRASS;
}

} // namespace


WallHit slide_to_wall(const StoryWorld& world, const TilePos& from, Direction direction){
    WallHit hit;
    hit.stop = from;
    while (true){
        const TilePos before = hit.stop;
        const std::optional<TilePos> next = step(world, before, direction);
        if (!next){
            hit.wall = before.next(direction);
            hit.anchored = at_wall(world, before, direction);
            return hit;
        }
        hit.tiles += distance(*next, before);
        hit.stop = *next;
    }
}


std::string to_string(const TileLeg& leg){
    return std::format("{} {}{}", Pokemon::direction_name(leg.direction), leg.tiles,
        leg.ends_at_wall ? " (to wall)" : leg.held ? " (held)" : " (open)");
}


namespace{

//  The world's step rules for the shared leg planner. A ledge hop is one
//  press that moves two tiles.
class StoryWorldModel{
public:
    using State = TilePos;
    using StateHash = std::hash<TilePos>;

    StoryWorldModel(const StoryWorld& world, const TilePlanCosts& costs)
        : m_world(world)
        , m_costs(costs)
    {}

    std::optional<Pokemon::Step<State>> press(const State& pos, Direction direction) const{
        const std::optional<TilePos> next = step(m_world, pos, direction);
        if (!next){
            return std::nullopt;
        }
        Pokemon::Step<State> step;
        step.next = *next;
        step.facing = direction;
        step.extra_cost = m_costs.per_tile * ((int)distance(*next, pos) - 1);
        if (tall_grass(m_world, *next)){
            step.extra_cost += m_costs.per_grass_tile;
        }
        if (wandering_hazard(m_world, *next)){
            step.extra_cost += m_costs.per_wander_tile;
        }
        return step;
    }
    bool held_at(const State& pos, Direction direction) const{
        return at_wall(m_world, pos, direction);
    }

private:
    const StoryWorld& m_world;
    const TilePlanCosts& m_costs;
};

void check_standable(const StoryWorld& world, const TilePos& from, const TilePos& to){
    if (!standable(world, from) || !standable(world, to)){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format(
            "Route ends must be standable: {} -> {}.", Pokemon::to_string(from), Pokemon::to_string(to)
        ));
    }
}

} // namespace

std::optional<std::vector<TileLeg>> plan_tile_route(
    const StoryWorld& world,
    const TilePos& from, const TilePos& to,
    const TilePlanCosts& costs
){
    check_standable(world, from, to);
    if (from == to){
        return std::vector<TileLeg>();
    }
    Pokemon::LegCosts leg_costs;
    leg_costs.per_tile = costs.per_tile;
    leg_costs.per_turn = costs.per_leg;
    leg_costs.per_open_stop = costs.leg_not_at_wall;
    const auto legs = Pokemon::plan_legs(
        StoryWorldModel(world, costs), from, std::nullopt,
        std::function<bool(const TilePos&)>([&](const TilePos& pos){ return pos == to; }),
        std::nullopt, leg_costs
    );
    if (!legs){
        return std::nullopt;
    }
    std::vector<TileLeg> ret;
    for (const Pokemon::Leg<TilePos>& leg : *legs){
        ret.push_back(TileLeg{leg.direction, distance(leg.end, leg.start), leg.to_wall, false});
    }
    return ret;
}

std::vector<TilePos> sight_tiles(const StoryWorld& world, const TrainerSight& trainer){
    std::vector<TilePos> tiles;
    TilePos pos = trainer.pos;
    for (int i = 0; i < trainer.range; i++){
        pos = pos.next(trainer.facing);
        if (!standable(world, pos)){
            break;
        }
        tiles.push_back(pos);
    }
    return tiles;
}

std::vector<std::vector<std::string>> legs_crossing_sight(
    const StoryWorld& world,
    const TilePos& from,
    const std::vector<TileLeg>& legs,
    const std::vector<TrainerSight>& trainers
){
    std::vector<std::vector<TilePos>> sights;
    for (const TrainerSight& trainer : trainers){
        sights.push_back(sight_tiles(world, trainer));
    }
    std::vector<std::vector<std::string>> crossings;
    TilePos pos = from;
    for (const TileLeg& leg : legs){
        const std::vector<TilePos> tiles = trace_legs(world, pos, {leg});
        std::vector<std::string> seen_by;
        for (size_t t = 0; t < trainers.size(); t++){
            const bool seen = std::any_of(tiles.begin() + 1, tiles.end(), [&](const TilePos& tile){
                return std::ranges::find(sights[t], tile) != sights[t].end();
            });
            if (seen){
                seen_by.push_back(trainers[t].name);
            }
        }
        crossings.push_back(std::move(seen_by));
        pos = tiles.back();
    }
    return crossings;
}

std::vector<TilePos> trace_legs(
    const StoryWorld& world,
    const TilePos& from,
    const std::vector<TileLeg>& legs
){
    std::vector<TilePos> tiles{from};
    for (const TileLeg& leg : legs){
        size_t travelled = 0;
        while (travelled < leg.tiles){
            const TilePos pos = tiles.back();
            const std::optional<TilePos> next = step(world, pos, leg.direction);
            if (!next){
                throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format(
                    "Leg {} is stopped at {}.", to_string(leg), Pokemon::to_string(pos)
                ));
            }
            travelled += distance(*next, pos);
            tiles.push_back(*next);
        }
        if (travelled != leg.tiles){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("Leg {} overshoots on a ledge hop.", to_string(leg)));
        }
    }
    return tiles;
}


std::string to_string(const PlannedLeg& leg){
    std::string out = to_string(leg.leg) + " to";
    for (const TilePos& pos : leg.after){
        out += " " + Pokemon::to_string(pos);
    }
    return out;
}

namespace{

//  The world's step rules for the shared robust planner. A ledge hop is one
//  step that counts two tiles.
class RobustWorldModel{
public:
    using State = TilePos;
    struct StateLess{
        bool operator()(const TilePos& a, const TilePos& b) const{
            return a.y != b.y ? a.y < b.y : a.x < b.x;
        }
    };

    explicit RobustWorldModel(const StoryWorld& world)
        : m_world(world)
    {}

    std::optional<State> step(const State& pos, Direction direction) const{
        return PokemonBDSP::step(m_world, pos, direction);
    }
    size_t step_tiles(const State& from, const State& to) const{
        return distance(from, to);
    }
    bool held_at(const State& pos, Direction direction) const{
        return at_wall(m_world, pos, direction);
    }
    bool wild(const State& pos) const{
        return tall_grass(m_world, pos);
    }
    bool wander(const State& pos) const{
        return wandering_hazard(m_world, pos);
    }

private:
    const StoryWorld& m_world;
};

//  Names every field of Pokemon::RobustLegCosts, in order, so it stops
//  compiling when the shared costs gain or lose one: add the new field to
//  robust_leg_costs() below (and to RobustPlanCosts if BDSP sets it), so it
//  can't silently keep the shared default.
constexpr bool robust_leg_costs_fields_copied(){
    [[maybe_unused]] const auto& [
        per_tile, per_wild_tile, per_wander_tile, per_leg, leg_not_at_wall,
        open_error, open_error_per, exact_open_tiles, per_open_tile,
        hold_open_legs, per_open_wild_tile,
        max_open_tiles, max_spread, max_searched
    ] = Pokemon::RobustLegCosts{};
    return true;
}
static_assert(robust_leg_costs_fields_copied());

//  BDSP's costs as the shared planner's, field by field. per_wander_tile is
//  BDSP's own (town_plan_costs() may raise it), never the shared default.
//  An open leg's error is open_hold_error(), the same at every length.
Pokemon::RobustLegCosts robust_leg_costs(const RobustPlanCosts& costs){
    Pokemon::RobustLegCosts ret;
    ret.per_tile = costs.per_tile;
    ret.per_wild_tile = costs.per_grass_tile;
    ret.per_wander_tile = costs.per_wander_tile;
    ret.per_leg = costs.per_leg;
    ret.leg_not_at_wall = costs.leg_not_at_wall;
    ret.open_error = (int)open_hold_error((size_t)costs.max_open_tiles);
    ret.open_error_per = 0;
    ret.exact_open_tiles = costs.exact_open_tiles;
    ret.per_open_tile = costs.per_open_tile;
    ret.hold_open_legs = costs.hold_open_legs;
    ret.per_open_wild_tile = costs.per_open_grass_tile;
    ret.max_open_tiles = costs.max_open_tiles;
    ret.max_spread = costs.max_spread;
    ret.max_searched = costs.max_searched;
    return ret;
}

} // namespace

size_t open_hold_error(size_t){
    return 1;
}

std::optional<std::vector<PlannedLeg>> plan_robust_route(
    const StoryWorld& world,
    const TilePos& from, const TilePos& to,
    const RobustPlanCosts& costs
){
    check_standable(world, from, to);
    return plan_robust_route(world, std::vector<TilePos>{from}, [&](const TilePos& pos){ return pos == to; }, costs);
}

std::optional<std::vector<PlannedLeg>> plan_robust_route(
    const StoryWorld& world,
    const std::vector<TilePos>& from, const std::function<bool(const TilePos&)>& goal,
    const RobustPlanCosts& costs
){
    for (const TilePos& pos : from){
        if (!standable(world, pos)){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Route starts must be standable: " + Pokemon::to_string(pos) + ".");
        }
    }
    const auto legs = Pokemon::plan_robust_legs(RobustWorldModel(world), from, goal, robust_leg_costs(costs));
    if (!legs){
        return std::nullopt;
    }
    std::vector<PlannedLeg> ret;
    for (const Pokemon::RobustLeg<TilePos>& leg : *legs){
        ret.push_back(PlannedLeg{
            .leg = {.direction = leg.direction, .tiles = leg.tiles, .ends_at_wall = leg.to_wall, .held = leg.held},
            .after = leg.after,
        });
    }
    return ret;
}

long long robust_route_cost(
    const StoryWorld& world,
    const std::vector<TilePos>& from, const std::vector<PlannedLeg>& legs,
    const RobustPlanCosts& costs
){
    std::vector<Pokemon::RobustLeg<TilePos>> robust;
    for (const PlannedLeg& planned : legs){
        robust.push_back(Pokemon::RobustLeg<TilePos>{
            .direction = planned.leg.direction,
            .tiles = planned.leg.tiles,
            .to_wall = planned.leg.ends_at_wall,
            .after = planned.after,
            .held = planned.leg.held,
        });
    }
    return Pokemon::robust_legs_cost(RobustWorldModel(world), from, robust, robust_leg_costs(costs));
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
