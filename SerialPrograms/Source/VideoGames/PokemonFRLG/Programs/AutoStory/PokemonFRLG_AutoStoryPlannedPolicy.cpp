/*  AutoStory Planned Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A line-for-line port of planned.py (PokemonSimulation #115, 837d757)
 *  without its riskier variants (goals["_risk"]): keep the two in step.
 *
 */

#include <algorithm>
#include <limits>
#include <numeric>
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryPlannedPolicy.h"
#include "PokemonFRLG_AutoStoryPlannedRisk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr int WHOLE_HEAL = std::numeric_limits<int>::max() / 2;
constexpr std::string_view FULL_RESTORE = "full-restore";
constexpr std::string_view GUARD_SPEC = "guard-spec";

int heal_hp(std::string_view item){
    if (item == "super-potion"){
        return 50;
    }
    if (item == "hyper-potion"){
        return 200;
    }
    if (item == "potion"){
        return 20;
    }
    if (item == "max-potion" || item == FULL_RESTORE){
        return WHOLE_HEAL;
    }
    return 0;
}

//  Cheapest first: what a sure KO spends.
int pp_value(std::string_view move){
    if (move == "strength"){
        return 0;
    }
    if (move == "bite"){
        return 1;
    }
    if (move == "surf"){
        return 2;
    }
    if (move == "blizzard"){
        return 9;
    }
    return 5;
}

int sum_of(const std::vector<int>& damage){
    return std::accumulate(damage.begin(), damage.end(), 0);
}

bool sure_ko(const PlannedMove& move, int hp){
    return move.accuracy >= 1 && !move.two_turn && !move.damage.empty() && move.min_damage() >= hp;
}

int stage_of(const PlannedOwn& own, std::string_view item){
    if (item == "x-special"){
        return own.spa_stage;
    }
    if (item == "x-speed"){
        return own.speed_stage;
    }
    if (item == "x-accuracy"){
        return own.accuracy_stage;
    }
    if (item == "x-attack"){
        return own.attack_stage;
    }
    if (item == "x-defend"){
        return own.defense_stage;
    }
    return 0;
}

const PlannedMove* own_move(const PlannedOwn& own, std::string_view id){
    for (const PlannedMove& move : own.moves){
        if (move.usable() && move.id == id){
            return &move;
        }
    }
    return nullptr;
}

std::optional<size_t> slot_of(const PlannedOwn& own, const PlannedMove* move){
    if (move == nullptr){
        return std::nullopt;
    }
    return static_cast<size_t>(move - own.moves.data());
}

//  The smallest heal that lifts HP above D and gains at least D plus a mean hit; else bigger.
std::optional<std::string> heal(const PlannedState& state, const PlannedDanger& danger, const LeagueFightData& fight){
    const PlannedOwn& own = state.own;
    const int missing = own.max_hp - own.hp;
    for (std::string_view name : {"super-potion", "hyper-potion", "max-potion"}){
        if (spendable(fight, state.bag, name) <= 0){
            continue;
        }
        const int gained = std::min(missing, heal_hp(name));
        if (own.hp + gained > danger.normal_max
            && (gained - danger.normal_max >= danger.mean || (own.hp > danger.normal_max && gained > danger.mean))
        ){
            return std::string(name);
        }
    }
    //  Over the reserve if need be: dying here loses the League anyway.
    if (count_of(state.bag, FULL_RESTORE) > 0){
        return std::string(FULL_RESTORE);
    }
    for (std::string_view name : {"max-potion", "hyper-potion", "super-potion"}){
        if (count_of(state.bag, name) > 0 && own.hp + std::min(missing, heal_hp(name)) > danger.normal_max){
            return std::string(name);
        }
    }
    return std::nullopt;
}

//  The first unmet setup goal whose served move still has PP and whose item is spendable.
const LeagueSetupGoal* setup_goal(const PlannedState& state, const LeagueFightData& fight, const LeagueFoePlan& plan){
    const PlannedOwn& own = state.own;
    for (const LeagueSetupGoal& goal : plan.setup){
        if (goal.serves){
            const PlannedMove* served = own_move(own, *goal.serves);
            if (served == nullptr || pp_above_reserve(fight, served->id, served->pp) <= 0){
                continue;
            }
        }
        if (spendable(fight, state.bag, goal.item) <= 0){
            continue;
        }
        if (goal.item == GUARD_SPEC){
            if (own.mist){
                continue;
            }
            return &goal;
        }
        if (stage_of(own, goal.item) < goal.stage){
            return &goal;
        }
    }
    return nullptr;
}

const PlannedMove* best_expected(const PlannedOwn& own, const LeagueFightData& fight, bool above_reserve){
    const PlannedMove* best = nullptr;
    for (const PlannedMove& move : own.moves){
        if (!move.usable() || move.damage.empty()){
            continue;
        }
        if (above_reserve && (move.max_damage() <= 0 || pp_above_reserve(fight, move.id, move.pp) <= 0)){
            continue;
        }
        if (best == nullptr || move.expected_hit() > best->expected_hit()){
            best = &move;
        }
    }
    return best;
}

//  The heal block's item: below its HP, the smallest spendable heal that
//  restores at least its amount (of what is missing).
std::optional<std::string> foe_heal(const PlannedState& state, const LeagueFightData& fight, const LeagueFoePlan& plan){
    const PlannedOwn& own = state.own;
    if (!plan.heal || own.hp >= plan.heal->below){
        return std::nullopt;
    }
    const int missing = own.max_hp - own.hp;
    for (std::string_view name : {"super-potion", "hyper-potion", "max-potion", "full-restore"}){
        if (spendable(fight, state.bag, name) > 0 && std::min(missing, heal_hp(name)) >= std::min(missing, plan.heal->heal_at_least)){
            return std::string(name);
        }
    }
    return std::nullopt;
}

//  The foe's sequence: our n-th attack on it uses its n-th move, while
//  that move has PP.
const PlannedMove* sequence_move(const PlannedState& state, const LeagueFoePlan& plan){
    const size_t attacks = static_cast<size_t>(std::max(0, state.attacks_on_foe));
    if (attacks >= plan.sequence.size()){
        return nullptr;
    }
    return own_move(state.own, plan.sequence[attacks]);
}

const PlannedMove* planned_move(const PlannedState& state, const LeagueFightData& fight, const LeagueFoePlan& plan){
    const PlannedOwn& own = state.own;
    if (const PlannedMove* move = sequence_move(state, plan)){
        return move;
    }
    for (const std::string& name : plan.moves){
        const PlannedMove* move = own_move(own, name);
        if (move != nullptr && pp_above_reserve(fight, move->id, move->pp) > 0){
            return move;
        }
    }
    //  Nothing planned has PP: the best expected hit among moves not held in reserve.
    const PlannedMove* best = best_expected(own, fight, true);
    return best != nullptr ? best : best_expected(own, fight, false);
}

//  With a trainer heal open, never drop the foe into [1, 1/4): an opener
//  whose every roll leaves it at or above 1/4 and after which the
//  finisher's lowest roll KOs.
const PlannedMove* kill_order(const PlannedState& state, const LeagueFightData& fight, const PlannedMove& chosen){
    const PlannedFoe& foe = state.foe;
    const int quarter = foe.max_hp / 4;
    const int hp = foe.hp;
    const std::vector<int>& rolls = chosen.damage;
    if (!state.trainer_item_open || rolls.empty() || chosen.min_damage() >= hp){
        return nullptr;
    }
    const bool into_heal_range = std::any_of(rolls.begin(), rolls.end(), [&](int roll){
        return 0 < hp - roll && hp - roll < quarter;
    });
    if (!into_heal_range){
        return nullptr;
    }
    std::vector<const PlannedMove*> moves;
    int finisher = 0;
    for (const PlannedMove& move : state.own.moves){
        if (move.usable() && move.max_damage() > 0 && pp_above_reserve(fight, move.id, move.pp) > 0){
            moves.push_back(&move);
            finisher = std::max(finisher, move.accuracy >= 1 ? move.min_damage() : 0);
        }
    }
    const auto strongest = [&](bool needs_finisher) -> const PlannedMove*{
        const PlannedMove* best = nullptr;
        for (const PlannedMove* move : moves){
            if (hp - move->max_damage() < quarter){
                continue;
            }
            if (needs_finisher && hp - move->min_damage() > finisher){
                continue;
            }
            if (best == nullptr || sum_of(move->damage) > sum_of(best->damage)){
                best = move;
            }
        }
        return best;
    };
    const PlannedMove* opener = strongest(true);
    if (opener == nullptr){
        //  No two-hit kill: the strongest hit that still leaves the foe at or above 1/4.
        opener = strongest(false);
    }
    return opener != nullptr && opener != &chosen ? opener : nullptr;
}

bool medicine(const PlannedChoice& choice){
    if (!choice.item){
        return false;
    }
    const std::string& item = *choice.item;
    return heal_hp(item) > 0 || item == "full-heal" || item == "awakening" || item == "antidote"
        || item == "parlyz-heal" || item == "burn-heal" || item == "ice-heal";
}

PlannedChoice item_choice(std::string_view row, std::string_view item){
    return {.row = std::string(row), .item = std::string(item)};
}

PlannedChoice move_choice(std::string_view row, std::optional<size_t> slot){
    return {.row = std::string(row), .move_slot = slot};
}

bool foe_moves_first_to_ko(const PlannedState& state, const PlannedMove& move){
    if (state.foe.recharging || state.foe.sleeping || state.foe.frozen){
        return false;
    }
    for (const PlannedMove& foe_move : state.foe.moves){
        if (!foe_move.usable() || foe_move.priority <= move.priority){
            continue;
        }
        if (foe_move.max_damage() >= state.own.hp){
            return true;
        }
    }
    return false;
}

//  sure_ko's row: the cheapest move that KOs for certain, moving first.
std::optional<PlannedChoice> sure_ko_row(const PlannedState& state, const LeagueFightData& fight, int danger){
    const PlannedOwn& own = state.own;
    const int target_hp = foe_hp_at_our_move(state);
    const PlannedMove* cheapest = nullptr;
    for (const PlannedMove& move : own.moves){
        if (!move.usable() || !sure_ko(move, target_hp) || !we_move_first(state, move) || foe_moves_first_to_ko(state, move)){
            continue;
        }
        if (pp_above_reserve(fight, move.id, move.pp) <= 0 && own.hp > danger){
            continue;
        }
        if (cheapest == nullptr || pp_value(move.id) < pp_value(cheapest->id)){
            cheapest = &move;
        }
    }
    if (cheapest == nullptr){
        return std::nullopt;
    }
    return move_choice("sure_ko", slot_of(own, cheapest));
}

std::optional<PlannedChoice> sleep_cure_row(const PlannedState& state, int danger){
    const PlannedOwn& own = state.own;
    if (own.hp <= danger && danger < own.max_hp && count_of(state.bag, FULL_RESTORE) > 0){
        return item_choice("sleep_cure", FULL_RESTORE);
    }
    const std::optional<std::string_view> cure = status_cure(own.status);
    for (std::optional<std::string_view> name : {cure, std::optional<std::string_view>("full-heal"), std::optional<std::string_view>(FULL_RESTORE)}){
        if (name && count_of(state.bag, *name) > 0){
            return item_choice("sleep_cure", *name);
        }
    }
    return std::nullopt;
}

std::optional<PlannedChoice> ko_risk_rows(
    const PlannedState& state, const LeagueFightData& fight, const PlannedDanger& danger, int streak
){
    const PlannedOwn& own = state.own;
    const bool has_restore = count_of(state.bag, FULL_RESTORE) > 0;
    if (own.status != PlannedStatus::NONE && has_restore){
        return item_choice("status_ko", FULL_RESTORE);
    }
    if (streak >= 2){
        if (has_restore){
            return item_choice("ko_heal", FULL_RESTORE);
        }
        return std::nullopt;
    }
    const std::optional<std::string> name = heal(state, danger, fight);
    if (name){
        return item_choice("ko_heal", *name);
    }
    return std::nullopt;
}

std::optional<PlannedChoice> cure_row(const PlannedState& state, const LeagueFightData& fight){
    switch (state.own.status){
    case PlannedStatus::PARALYSIS:
    case PlannedStatus::POISON:
    case PlannedStatus::BURN:
        break;
    default:
        return std::nullopt;
    }
    for (std::optional<std::string_view> name : {status_cure(state.own.status), std::optional<std::string_view>("full-heal")}){
        if (name && spendable(fight, state.bag, *name) > 0){
            return item_choice("cure", *name);
        }
    }
    return std::nullopt;
}

//  Every planned move is out of PP above its reserve.
bool plan_out_of_pp(const PlannedState& state, const LeagueFightData& fight, const LeagueFoePlan& plan){
    for (const PlannedMove& move : state.own.moves){
        if (!move.usable() || std::find(plan.moves.begin(), plan.moves.end(), move.id) == plan.moves.end()){
            continue;
        }
        if (pp_above_reserve(fight, move.id, move.pp) > 0){
            return false;
        }
    }
    return true;
}

std::optional<PlannedChoice> setup_row(
    const PlannedState& state, const LeagueFightData& fight, const LeagueFoePlan& plan, const PlannedDanger& danger
){
    const LeagueSetupGoal* goal = setup_goal(state, fight, plan);
    if (goal == nullptr){
        return std::nullopt;
    }
    const PlannedOwn& own = state.own;
    const int turns = static_cast<int>(std::count_if(plan.setup.begin(), plan.setup.end(), [](const LeagueSetupGoal& g){
        return g.item != GUARD_SPEC;
    }));
    const double keep = std::min(own.max_hp * 0.75, danger.normal_max + std::max(0, turns - 1) * danger.mean);
    if (own.hp >= keep || foe_hp_at_our_move(state) != state.foe.hp){
        return item_choice("plan:setup", goal->item);
    }
    const std::optional<std::string> name = heal(state, danger, fight);
    if (name && (*name != FULL_RESTORE || spendable(fight, state.bag, FULL_RESTORE) > 0)){
        return item_choice("plan:setup_heal", *name);
    }
    return std::nullopt;
}

//  The rows after KO risk and cures: pp_restore, the plan's setup and
//  moves, and kill_order.
PlannedChoice plan_rows(const PlannedState& state, const LeagueFightData& fight, const LeagueFoePlan& plan, const PlannedDanger& danger){
    const PlannedOwn& own = state.own;
    const bool risk = own.hp <= danger.normal_max;
    const PlannedMove* chosen = planned_move(state, fight, plan);
    if ((chosen == nullptr || plan_out_of_pp(state, fight, plan)) && !risk && !plan.moves.empty()){
        for (std::string_view name : {"elixir", "max-elixir"}){
            if (spendable(fight, state.bag, name) > 0){
                return item_choice("pp_restore", name);
            }
        }
    }
    if (!risk){
        if (std::optional<PlannedChoice> choice = setup_row(state, fight, plan, danger)){
            return *choice;
        }
    }
    if (chosen == nullptr){
        return move_choice("plan:struggle", std::nullopt);
    }
    const bool awake = own.status != PlannedStatus::SLEEP && own.status != PlannedStatus::FREEZE;
    if (awake && plan.kill_order && plan.sequence.empty()){
        if (const PlannedMove* opener = kill_order(state, fight, *chosen)){
            return move_choice("kill_order", slot_of(own, opener));
        }
    }
    return move_choice("plan:move", slot_of(own, chosen));
}

} // namespace


PlannedChoice choose_planned(const PlannedState& state, const LeagueFightData& fight, int streak){
    const PlannedOwn& own = state.own;
    if (own.locked_slot){
        return move_choice("locked", own.locked_slot);
    }
    const LeagueFoePlan* found = league_foe_plan(fight, state.foe.slot, state.foe.species);
    const LeagueFoePlan plan = found != nullptr ? *found : LeagueFoePlan{.species = state.foe.species};
    const PlannedDanger danger = planned_danger(state);
    const int d = danger.normal_max;
    const bool awake = own.status != PlannedStatus::SLEEP && own.status != PlannedStatus::FREEZE;
    //  A sure KO waits while this foe is the plan's setup foe and we are
    //  safe: the setup carries to the next foes.
    const bool goal_first = own.hp > d && setup_goal(state, fight, plan) != nullptr && state.foe_party_alive > 1;
    if (plan.heal && plan.heal->heal_when_can_ko && streak < 2){
        if (std::optional<std::string> name = foe_heal(state, fight, plan)){
            return item_choice("foe_heal", *name);
        }
    }
    if (awake && !goal_first){
        if (std::optional<PlannedChoice> choice = sure_ko_row(state, fight, d)){
            return *choice;
        }
    }
    if (!awake){
        if (std::optional<PlannedChoice> choice = sleep_cure_row(state, d)){
            return *choice;
        }
    }
    if (plan.heal && streak < 2){
        if (std::optional<std::string> name = foe_heal(state, fight, plan)){
            return item_choice("foe_heal", *name);
        }
    }
    const bool risk = own.hp <= d;
    if (risk && d < own.max_hp && (!plan.heal || plan.heal->ko_rule)){
        if (std::optional<PlannedChoice> choice = ko_risk_rows(state, fight, danger, streak)){
            return *choice;
        }
    }
    if (!risk && streak == 0){
        if (std::optional<PlannedChoice> choice = cure_row(state, fight)){
            return *choice;
        }
    }
    return plan_rows(state, fight, plan, danger);
}

int PlannedStreak::streak(const PlannedState& state) const{
    return state.foe.species == m_species && state.foe.hp == m_hp ? m_streak : 0;
}

int PlannedStreak::attacks(size_t foe_slot) const{
    const auto it = m_attacks.find(foe_slot);
    return it == m_attacks.end() ? 0 : it->second;
}

void PlannedStreak::chose(const PlannedState& state, const PlannedChoice& choice){
    //  Every row but an item's is an attack (plan:struggle's too).
    if (!choice.item){
        m_attacks[state.foe.slot]++;
    }
    const int before = streak(state);
    m_species = state.foe.species;
    m_hp = state.foe.hp;
    m_streak = medicine(choice) ? before + 1 : 0;
}




} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
