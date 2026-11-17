/*  AutoStory Planned Risk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  planned.py's foe_hp_at_our_move(), danger() and we_move_first()
 *  (PokemonSimulation #111, 4a79ccc): keep the two in step.
 *
 */

#include <algorithm>
#include "PokemonFRLG_AutoStoryPlannedRisk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr int MAX_TRAINER_ITEMS = 4;

bool trainer_uses_item(const PlannedState& state){
    return state.trainer_item_open && state.foe.hp > 0 && state.foe.hp < state.foe.max_hp / 4;
}

} // namespace


bool trainer_item_open(int carried, int used, int alive){
    for (int slot = used; slot < carried; slot++){
        if (slot == 0 || alive <= MAX_TRAINER_ITEMS - slot + 1){
            return true;
        }
    }
    return false;
}

int foe_hp_at_our_move(const PlannedState& state){
    return trainer_uses_item(state) ? state.foe.max_hp : state.foe.hp;
}

PlannedDanger planned_danger(const PlannedState& state){
    const PlannedOwn& own = state.own;
    const PlannedFoe& foe = state.foe;
    const int residual = own.max_hp / 8 * ((own.status == PlannedStatus::POISON ? 1 : 0) + (own.leech_seed ? 1 : 0));
    if (foe.recharging || foe.sleeping || foe.frozen || trainer_uses_item(state)){
        return {.normal_max = residual, .mean = static_cast<double>(residual)};
    }
    int worst = 0;
    double mean = 0;
    for (const PlannedMove& move : foe.moves){
        if (!move.usable() || move.max_damage() <= 0){
            continue;
        }
        const bool charging = move.two_turn && foe.charging == move.id;
        if (move.two_turn && !charging && !(foe.sun && move.id == "solar-beam")){
            continue;
        }
        worst = std::max(worst, move.max_damage());
        mean = std::max(mean, move.expected_hit());
    }
    return {.normal_max = residual + worst, .mean = residual + mean};
}

bool we_move_first(const PlannedState& state, const PlannedMove& move){
    if (move.priority != 0){
        return move.priority > 0;
    }
    if (state.foe.recharging){
        return true;
    }
    return state.own.speed > state.foe.speed;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
