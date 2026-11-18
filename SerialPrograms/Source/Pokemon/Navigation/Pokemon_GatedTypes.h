/*  Gated Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the gated planner (Pokemon_GatedPlanner.h) shares with the games
 *  that plan with it: its actions, rules, and search state.
 *
 */

#ifndef PokemonAutomation_Pokemon_GatedTypes_H
#define PokemonAutomation_Pokemon_GatedTypes_H

#include "Pokemon_GateState.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The Step::action of the legs the wrapper adds. A game's own actions must
//  differ from these.
constexpr int ACTION_OPEN_GATE = 1000;
//  See ACTION_OPEN_GATE: the leg that presses a statue.
constexpr int ACTION_PRESS_SWITCH = 1001;

//  What the story lets a plan do, and what it costs.
struct GateRules{
    //  The story state says the player holds the Card Key: a plan may open card-key gates.
    bool card_key_held = false;
    //  Cost of opening a gate or pressing a statue, on top of LegCosts::per_tile:
    //  the dialog, and for a statue its yes/no.
    int open_gate_cost = 10;
    int press_switch_cost = 20;
};


//  The game's state and the gate state.
template <typename InnerState>
struct GatedState{
    InnerState inner;
    GateState gates;

    bool operator==(const GatedState& other) const = default;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
