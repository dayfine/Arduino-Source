/*  Overworld Robust Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <tuple>
#include "Pokemon/Navigation/Pokemon_RobustLegPlanner.h"
#include "PokemonFRLG_OverworldObjects.h"
#include "PokemonFRLG_OverworldRobustRoute.h"
#include "PokemonFRLG_OverworldWildTiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct RobustState{
    PlayerState state;

    bool operator==(const RobustState& other) const{
        return state.position == other.state.position
            && state.elevation == other.state.elevation
            && state.surfing == other.state.surfing;
    }
};

struct RobustStateLess{
    bool operator()(const RobustState& a, const RobustState& b) const{
        return std::tie(a.state.position.x, a.state.position.y, a.state.elevation, a.state.surfing)
            < std::tie(b.state.position.x, b.state.position.y, b.state.elevation, b.state.surfing);
    }
};

class RobustStepModel{
public:
    using State = RobustState;
    using StateLess = RobustStateLess;

    RobustStepModel(const MapData& map, const RobustRouteOps& ops)
        : m_map(map)
        , m_ops(ops)
    {}

    std::optional<State> step(const State& s, Direction direction) const{
        const std::optional<PlayerState> next = m_ops.walk_step(s.state, direction);
        return next ? std::optional<State>(RobustState{*next}) : std::nullopt;
    }
    size_t step_tiles(const State&, const State&) const{
        return 1;
    }
    bool held_at(const State& s, Direction direction) const{
        return m_ops.held_at(s.state, direction);
    }
    bool wild(const State& s) const{
        return is_tall_grass(m_map, s.state.position) || s.state.surfing;
    }
    bool wander(const State& s) const{
        return wander_object_at(m_map, s.state.position) != nullptr;
    }

private:
    const MapData& m_map;
    const RobustRouteOps& m_ops;
};

Pokemon::RobustLegCosts to_robust_leg_costs(const RouteCosts& costs, bool forbid_along_wander){
    Pokemon::RobustLegCosts robust;
    robust.per_tile = route_cost(costs, RouteCost::TILE);
    robust.per_wild_tile = route_cost(costs, RouteCost::GRASS_TILE);
    robust.per_wander_tile = route_cost(costs, RouteCost::WANDER_TILE);
    robust.per_leg = route_cost(costs, RouteCost::TURN);
    robust.leg_not_at_wall = route_cost(costs, RouteCost::OPEN_STOP);
    robust.per_open_wild_tile = route_cost(costs, RouteCost::OPEN_WILD_STOP);
    robust.open_error = 1;
    robust.open_error_per = 5;
    robust.forbid_along_wander = forbid_along_wander;
    return robust;
}

std::optional<RouteStep> walk_robust_leg(
    const MapData& map,
    const RobustRouteOps& ops,
    const Pokemon::RobustLeg<RobustState>& rleg,
    PlayerState& current
){
    const PlayerState start = current;
    int tiles = 0;
    bool wild = false;
    for (size_t i = 0; i < rleg.tiles; i++){
        const std::optional<PlayerState> next = ops.walk_step(current, rleg.direction);
        if (!next){
            if (rleg.to_wall){
                break;
            }
            return std::nullopt;
        }
        current = *next;
        wild |= is_tall_grass(map, current.position) || current.surfing;
        tiles++;
    }
    return ops.build_step(start, current, rleg.direction, tiles, rleg.to_wall, wild);
}

void append_goal_facing(
    const MapData& map,
    const PlayerState& current,
    const RouteGoal& goal,
    std::optional<Direction> start_facing,
    std::vector<RouteStep>& route
){
    if (!goal.facing){
        return;
    }
    const std::optional<Direction> facing = route.empty()
        ? start_facing
        : std::optional<Direction>(route.back().facing_after);
    if (!facing || *facing != *goal.facing){
        RouteStep face;
        face.action = RouteAction::FACE;
        face.direction = *goal.facing;
        face.walk_only = !map.allow_running;
        face.facing_after = *goal.facing;
        face.end = MapPosition{map.name, current.position};
        route.push_back(std::move(face));
    }
}

std::optional<std::vector<RouteStep>> robust_legs_to_route(
    const MapData& map,
    const RobustRouteOps& ops,
    const PlayerState& first,
    const std::vector<Pokemon::RobustLeg<RobustState>>& legs,
    const RouteGoal& goal,
    std::optional<Direction> start_facing
){
    std::vector<RouteStep> route;
    PlayerState current = first;
    for (const Pokemon::RobustLeg<RobustState>& rleg : legs){
        std::optional<RouteStep> step = walk_robust_leg(map, ops, rleg, current);
        if (!step){
            return std::nullopt;
        }
        route.push_back(std::move(*step));
    }
    append_goal_facing(map, current, goal, start_facing, route);
    tap_from_known_tile(route);
    return route;
}

} // namespace


void tap_from_known_tile(std::vector<RouteStep>& route){
    for (size_t c = route.size(); c-- > 1;){
        if (!route[c].tapped){
            continue;
        }
        for (size_t before = c; before-- > 0;){
            RouteStep& step = route[before];
            if (step.action == RouteAction::FACE){
                continue;
            }
            if (step.action != RouteAction::WALK || step.surfing || step.warps || step.to_wall || step.spin_tiles || step.tapped){
                break;
            }
            step.tapped = true;
        }
    }
}

std::optional<std::vector<RouteStep>> plan_robust_route_on(
    const MapData& map,
    const PlayerState& first,
    const RouteGoal& goal,
    const RouteCosts& costs,
    std::optional<Direction> start_facing,
    const RobustRouteOps& ops
){
    const bool eligible = !first.surfing
        && route_cost(costs, RouteCost::SIGHT_TILE) == 0
        && route_cost(costs, RouteCost::LINE_TILE) == 0
        && route_cost(costs, RouteCost::SIGHT_OVERSHOOT) == 0;
    if (!eligible){
        return std::nullopt;
    }
    const RobustStepModel robust_model(map, ops);
    const std::function<bool(const RobustState&)> at_goal = [&](const RobustState& player){
        return player.state.position == goal.position.point;
    };
    for (bool forbid_along_wander : {true, false}){
        const auto legs = Pokemon::plan_robust_legs(
            robust_model, {RobustState{first}}, at_goal, to_robust_leg_costs(costs, forbid_along_wander)
        );
        if (legs){
            if (auto route = robust_legs_to_route(map, ops, first, *legs, goal, start_facing)){
                return route;
            }
        }
    }
    return std::nullopt;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
