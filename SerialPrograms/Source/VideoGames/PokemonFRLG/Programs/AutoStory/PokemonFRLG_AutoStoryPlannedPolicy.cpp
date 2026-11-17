/*  AutoStory Planned Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A line-for-line port of planned.py (PokemonSimulation #111, 4a79ccc):
 *  keep the two in step.
 *
 */

#include <algorithm>
#include <limits>
#include <numeric>
#include "PokemonFRLG_AutoStoryPlannedPolicy.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr int MAX_TRAINER_ITEMS = 4;
constexpr int WHOLE_HEAL = std::numeric_limits<int>::max() / 2;
constexpr std::string_view FULL_RESTORE = "full-restore";
constexpr std::string_view GUARD_SPEC = "guard-spec";
//  What an Elixir restores to each move.
constexpr int ELIXIR_PP = 10;

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

std::optional<std::string_view> cure_of(PlannedStatus status){
    switch (status){
    case PlannedStatus::SLEEP:
        return "awakening";
    case PlannedStatus::POISON:
        return "antidote";
    case PlannedStatus::PARALYSIS:
        return "parlyz-heal";
    case PlannedStatus::BURN:
        return "burn-heal";
    case PlannedStatus::FREEZE:
        return "ice-heal";
    case PlannedStatus::NONE:
        return std::nullopt;
    }
    return std::nullopt;
}

int bag_has(const SlugCounts& bag, std::string_view item){
    const auto it = bag.find(item);
    return it == bag.end() ? 0 : it->second;
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

double mean_of(const std::vector<int>& damage){
    if (damage.empty()){
        return 0;
    }
    return static_cast<double>(std::accumulate(damage.begin(), damage.end(), 0)) / static_cast<double>(damage.size());
}

int max_of(const std::vector<int>& damage){
    return damage.empty() ? 0 : damage.back();
}

int min_of(const std::vector<int>& damage){
    return damage.empty() ? 0 : damage.front();
}

int sum_of(const std::vector<int>& damage){
    return std::accumulate(damage.begin(), damage.end(), 0);
}

bool usable(const PlannedMove& move){
    return move.pp > 0;
}

bool sure_ko(const PlannedMove& move, int hp){
    return move.accuracy >= 1 && !move.two_turn && !move.damage.empty() && min_of(move.damage) >= hp;
}

bool trainer_uses_item(const PlannedState& state){
    return state.trainer_item_open && state.foe.hp > 0 && state.foe.hp < state.foe.max_hp / 4;
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
        if (usable(move) && move.id == id){
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
        if (own.hp + gained > danger.normal_max && gained - danger.normal_max >= danger.mean){
            return std::string(name);
        }
    }
    //  Over the reserve if need be: dying here loses the League anyway.
    if (bag_has(state.bag, FULL_RESTORE) > 0){
        return std::string(FULL_RESTORE);
    }
    for (std::string_view name : {"max-potion", "hyper-potion", "super-potion"}){
        if (bag_has(state.bag, name) > 0 && own.hp + std::min(missing, heal_hp(name)) > danger.normal_max){
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

double expected_hit(const PlannedMove& move){
    return mean_of(move.damage) * move.accuracy;
}

const PlannedMove* best_expected(const PlannedOwn& own, const LeagueFightData& fight, bool above_reserve){
    const PlannedMove* best = nullptr;
    for (const PlannedMove& move : own.moves){
        if (!usable(move) || move.damage.empty()){
            continue;
        }
        if (above_reserve && (max_of(move.damage) <= 0 || pp_above_reserve(fight, move.id, move.pp) <= 0)){
            continue;
        }
        if (best == nullptr || expected_hit(move) > expected_hit(*best)){
            best = &move;
        }
    }
    return best;
}

const PlannedMove* planned_move(const PlannedState& state, const LeagueFightData& fight, const LeagueFoePlan& plan){
    const PlannedOwn& own = state.own;
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
    if (!state.trainer_item_open || rolls.empty() || min_of(rolls) >= hp){
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
        if (usable(move) && max_of(move.damage) > 0 && pp_above_reserve(fight, move.id, move.pp) > 0){
            moves.push_back(&move);
            finisher = std::max(finisher, move.accuracy >= 1 ? min_of(move.damage) : 0);
        }
    }
    const auto strongest = [&](bool needs_finisher) -> const PlannedMove*{
        const PlannedMove* best = nullptr;
        for (const PlannedMove* move : moves){
            if (hp - max_of(move->damage) < quarter){
                continue;
            }
            if (needs_finisher && hp - min_of(move->damage) > finisher){
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

//  sure_ko's row: the cheapest move that KOs for certain, moving first.
std::optional<PlannedChoice> sure_ko_row(const PlannedState& state, const LeagueFightData& fight, int danger){
    const PlannedOwn& own = state.own;
    const int target_hp = foe_hp_at_our_move(state);
    const PlannedMove* cheapest = nullptr;
    for (const PlannedMove& move : own.moves){
        if (!usable(move) || !sure_ko(move, target_hp) || !we_move_first(state, move)){
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
    if (own.hp <= danger && danger < own.max_hp && bag_has(state.bag, FULL_RESTORE) > 0){
        return item_choice("sleep_cure", FULL_RESTORE);
    }
    const std::optional<std::string_view> cure = cure_of(own.status);
    for (std::optional<std::string_view> name : {cure, std::optional<std::string_view>("full-heal"), std::optional<std::string_view>(FULL_RESTORE)}){
        if (name && bag_has(state.bag, *name) > 0){
            return item_choice("sleep_cure", *name);
        }
    }
    return std::nullopt;
}

std::optional<PlannedChoice> ko_risk_rows(
    const PlannedState& state, const LeagueFightData& fight, const PlannedDanger& danger, int streak
){
    const PlannedOwn& own = state.own;
    const bool has_restore = bag_has(state.bag, FULL_RESTORE) > 0;
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
    for (std::optional<std::string_view> name : {cure_of(state.own.status), std::optional<std::string_view>("full-heal")}){
        if (name && spendable(fight, state.bag, *name) > 0){
            return item_choice("cure", *name);
        }
    }
    return std::nullopt;
}

//  Every planned move is out of PP above its reserve.
bool plan_out_of_pp(const PlannedState& state, const LeagueFightData& fight, const LeagueFoePlan& plan){
    for (const PlannedMove& move : state.own.moves){
        if (!usable(move) || std::find(plan.moves.begin(), plan.moves.end(), move.id) == plan.moves.end()){
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
        if (!usable(move) || max_of(move.damage) <= 0){
            continue;
        }
        const bool charging = move.two_turn && foe.charging == move.id;
        if (move.two_turn && !charging && !(foe.sun && move.id == "solar-beam")){
            continue;
        }
        worst = std::max(worst, max_of(move.damage));
        mean = std::max(mean, expected_hit(move));
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
    const bool risk = own.hp <= d;
    if (risk && d < own.max_hp){
        if (std::optional<PlannedChoice> choice = ko_risk_rows(state, fight, danger, streak)){
            return *choice;
        }
    }
    if (!risk && streak == 0){
        if (std::optional<PlannedChoice> choice = cure_row(state, fight)){
            return *choice;
        }
    }
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
    if (awake){
        if (const PlannedMove* opener = kill_order(state, fight, *chosen)){
            return move_choice("kill_order", slot_of(own, opener));
        }
    }
    return move_choice("plan:move", slot_of(own, chosen));
}

int PlannedStreak::streak(const PlannedState& state) const{
    return state.foe.species == m_species && state.foe.hp == m_hp ? m_streak : 0;
}

void PlannedStreak::chose(const PlannedState& state, const PlannedChoice& choice){
    const int before = streak(state);
    m_species = state.foe.species;
    m_hp = state.foe.hp;
    m_streak = medicine(choice) ? before + 1 : 0;
}

std::vector<std::string> planned_between(PlannedCarry& carry, const LeagueFightData& next){
    std::vector<std::string> used;
    const auto can = [&](std::string_view name){
        return spendable(next, carry.bag, name) > 0;
    };
    const auto use = [&](std::string_view name){
        carry.bag[std::string(name)]--;
        used.emplace_back(name);
    };
    if (carry.status != PlannedStatus::NONE){
        for (std::optional<std::string_view> name : {cure_of(carry.status), std::optional<std::string_view>("full-heal"), std::optional<std::string_view>(FULL_RESTORE)}){
            if (name && can(*name)){
                use(*name);
                carry.status = PlannedStatus::NONE;
                if (*name == FULL_RESTORE){
                    carry.hp = carry.max_hp;
                }
                break;
            }
        }
    }
    const double share = next.between_hp.value_or(1.0);
    const int target = static_cast<int>(carry.max_hp * share + 0.999);
    for (int c = 0; c < 20 && carry.hp < target; c++){
        if (can("super-potion") && (target - carry.hp <= 50 * bag_has(carry.bag, "super-potion") || !can(FULL_RESTORE))){
            use("super-potion");
            carry.hp = std::min(carry.max_hp, carry.hp + 50);
        }else if (can(FULL_RESTORE)){
            use(FULL_RESTORE);
            carry.hp = carry.max_hp;
        }else{
            break;
        }
    }
    const bool under_floor = std::any_of(next.between_pp.begin(), next.between_pp.end(), [&](const auto& floor){
        return bag_has(carry.pp, floor.first) < floor.second;
    });
    if (!under_floor){
        return used;
    }
    for (std::string_view name : {"elixir", "max-elixir"}){
        if (!can(name)){
            continue;
        }
        use(name);
        for (auto& [move, pp] : carry.pp){
            const int full = bag_has(carry.full_pp, move);
            pp = name == "max-elixir" ? full : std::min(full, pp + ELIXIR_PP);
        }
        break;
    }
    return used;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
