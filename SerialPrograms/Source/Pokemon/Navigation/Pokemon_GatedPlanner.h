/*  Gated Planner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plans legs (Pokemon_LegPlanner.h) over maps with gates (Pokemon_GateState.h).
 *  The game's step model walks the maps with a gate state; this wraps it, so
 *  the search state is the game's state and the gate state:
 *
 *      -  a closed gate is a wall, so a route goes round it;
 *      -  a closed card-key gate the player faces can be opened (an
 *         ACTION_OPEN_GATE leg: face it, press A), if the Card Key is held; the
 *         state after the leg has it open, and the route may go through;
 *      -  a statue faced from its tile can be pressed (an ACTION_PRESS_SWITCH
 *         leg), which flips the switch, so every gate that follows it changes
 *         with it, on every floor. A route may flip it, and flip it back.
 *
 *  The game's step model, as for plan_legs() but taking the gate state, and
 *  telling where its state is:
 *
 *      using State = ...;
 *      using StateHash = ...;
 *      std::optional<Step<State>> press(const State&, Direction, const GateState&) const;
 *      bool held_at(const State&, Direction, const GateState&) const;
 *      MapPlace place_of(const State&) const;
 *
 *  press() reads each tile with resolved_tile(), which makes a closed gate a
 *  wall. One-way warps (MapWarp::one_way) are the game's to take as any warp:
 *  the model leads the player to the warp's arrival, and the maps give no way
 *  back from it, so a route takes it forward only.
 *
 */

#ifndef PokemonAutomation_Pokemon_GatedPlanner_H
#define PokemonAutomation_Pokemon_GatedPlanner_H

#include <functional>
#include <optional>
#include <vector>
#include "Pokemon_ConnectedMaps.h"
#include "Pokemon_GateTiles.h"
#include "Pokemon_GatedTypes.h"
#include "Pokemon_LegPlanner.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The game's step model, wrapped: see the top of this file.
template <typename Inner>
class GatedModel{
public:
    using InnerState = typename Inner::State;
    using State = GatedState<InnerState>;

    struct StateHash{
        size_t operator()(const State& state) const{
            return typename Inner::StateHash()(state.inner) * 31 + std::hash<GateState>()(state.gates);
        }
    };

    //  `inner` and `world` must outlive the model.
    GatedModel(const Inner& inner, const ConnectedMaps& world, const GateRules& rules)
        : m_inner(inner)
        , m_world(world)
        , m_rules(rules)
    {}

    std::optional<Step<State>> press(const State& state, Direction direction) const{
        if (std::optional<Step<InnerState>> walked = m_inner.press(state.inner, direction, state.gates)){
            return carry(*walked, state.gates);
        }
        const MapPlace place = m_inner.place_of(state.inner);
        const MapData& map = m_world.map(place.map);
        if (m_rules.card_key_held){
            if (const MapGate* gate = closed_card_key_gate_faced(map, place.tile, direction, state.gates)){
                return open_gate(state, map, *gate, direction);
            }
        }
        if (const MapSwitch* pressed = switch_pressed(map, place.tile, direction)){
            return press_switch(state, *pressed, direction);
        }
        return std::nullopt;
    }

    bool held_at(const State& state, Direction direction) const{
        return m_inner.held_at(state.inner, direction, state.gates);
    }

private:
    static Step<State> carry(const Step<InnerState>& walked, const GateState& gates){
        Step<State> step;
        step.next = State{walked.next, gates};
        step.action = walked.action;
        step.starts_leg = walked.starts_leg;
        step.ends_leg = walked.ends_leg;
        step.warped = walked.warped;
        step.facing = walked.facing;
        step.extra_cost = walked.extra_cost;
        step.wild = walked.wild;
        return step;
    }

    //  The player stays where they are, facing the gate, which is open afterwards.
    Step<State> open_gate(const State& state, const MapData& map, const MapGate& gate, Direction direction) const{
        Step<State> step;
        step.next = state;
        step.next.gates.opened.insert(gate_key(map.name, gate.id));
        step.action = ACTION_OPEN_GATE;
        step.ends_leg = true;
        step.facing = direction;
        step.extra_cost = m_rules.open_gate_cost;
        return step;
    }

    Step<State> press_switch(const State& state, const MapSwitch& pressed, Direction direction) const{
        Step<State> step;
        step.next = state;
        const bool now_on = pressed.toggle ? !state.gates.switch_on(pressed.id) : true;
        if (now_on){
            step.next.gates.switches_on.insert(pressed.id);
        }else{
            step.next.gates.switches_on.erase(pressed.id);
        }
        step.action = ACTION_PRESS_SWITCH;
        step.ends_leg = true;
        step.facing = direction;
        step.extra_cost = m_rules.press_switch_cost;
        return step;
    }

    const Inner& m_inner;
    const ConnectedMaps& m_world;
    GateRules m_rules;
};


//  Where a plan starts: the game's state, the gates, and the way the player faces if known.
template <typename InnerState>
struct GatedStart{
    InnerState state;
    GateState gates;
    std::optional<Direction> facing = std::nullopt;
};

//  Where a plan ends: a state of the game where `at_goal` holds, facing `facing` if given.
template <typename InnerState>
struct GatedGoal{
    std::function<bool(const InnerState&)> at_goal;
    std::optional<Direction> facing = std::nullopt;
};

//  The cheapest legs from `start` to `goal` (as plan_legs()), or nothing.
//  Each leg's start and end carry the gate state; gate_change_of() says what
//  an ACTION leg did to it.
template <typename Inner>
std::optional<std::vector<Leg<GatedState<typename Inner::State>>>> plan_gated_legs(
    const Inner& inner, const ConnectedMaps& world, const GateRules& rules,
    const GatedStart<typename Inner::State>& start,
    const GatedGoal<typename Inner::State>& goal,
    const LegCosts& costs = LegCosts()
);

//  What a leg did to the gates: the gate it opened, the switch it flipped.
template <typename InnerState>
GateChange gate_change_of(const Leg<GatedState<InnerState>>& leg);

//  The gate state after the last of `legs`, or `before` if there are none.
template <typename InnerState>
GateState gates_after(const GateState& before, const std::vector<Leg<GatedState<InnerState>>>& legs);


template <typename Inner>
std::optional<std::vector<Leg<GatedState<typename Inner::State>>>> plan_gated_legs(
    const Inner& inner, const ConnectedMaps& world, const GateRules& rules,
    const GatedStart<typename Inner::State>& start,
    const GatedGoal<typename Inner::State>& goal,
    const LegCosts& costs
){
    using State = GatedState<typename Inner::State>;
    const GatedModel<Inner> model(inner, world, rules);
    return plan_legs(
        model, State{start.state, start.gates}, start.facing,
        std::function<bool(const State&)>([&](const State& state){
            return goal.at_goal(state.inner);
        }),
        goal.facing, costs
    );
}

template <typename InnerState>
GateChange gate_change_of(const Leg<GatedState<InnerState>>& leg){
    return gate_change_between(leg.start.gates, leg.end.gates);
}

template <typename InnerState>
GateState gates_after(const GateState& before, const std::vector<Leg<GatedState<InnerState>>>& legs){
    return legs.empty() ? before : legs.back().end.gates;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
