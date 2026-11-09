/*  AutoStory Battle Strategy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryBattleStrategy.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr int MAX_STAGE = 6;

//  No setup turn while the foe's next turn is a status move this often: the
//  turn hands it the status (Erika's Vileplume, Koga's Venomoth).
constexpr double MAX_SETUP_STATUS_CHANCE = 0.5;

//  HP each heal restores; a Full Restore all of it.
constexpr uint16_t SUPER_POTION_HP = 50;
constexpr uint16_t HYPER_POTION_HP = 200;

//  The heals, smallest first.
constexpr std::array<PlanItem, 3> HEALS = {PlanItem::SUPER_POTION, PlanItem::HYPER_POTION, PlanItem::FULL_RESTORE};

using GoalTable = std::map<std::string, SetupGoals, std::less<>>;

int8_t stage_goal(const JsonObject& entry, const std::string& key){
    return static_cast<int8_t>(std::clamp<int64_t>(entry.get_integer_default(key), 0, MAX_STAGE));
}

const GoalTable& goal_table(){
    static const GoalTable TABLE = []{
        GoalTable table;
        for (const auto& [file, text] : autostory_battle_json()){
            if (file != "strategies"){
                continue;
            }
            const JsonValue json = parse_json(text);
            for (const auto& [trainer, value] : json.to_object_throw()){
                const JsonObject& entry = value.to_object_throw();
                table.emplace(trainer, SetupGoals{
                    .attack = stage_goal(entry, "attack"),
                    .sp_attack = stage_goal(entry, "sp_attack"),
                    .accuracy = stage_goal(entry, "accuracy"),
                    .outspeed = entry.get_boolean_default("outspeed"),
                });
            }
        }
        return table;
    }();
    return TABLE;
}

int stage_of(const SimBattler& battler, Stage stat){
    return battler.body.stages[static_cast<size_t>(stat)];
}

uint32_t staged_speed(const SimBattler& battler){
    const uint32_t speed = staged_stat(battler.body.stats.speed, stage_of(battler, Stage::SPEED));
    return battler.status == SimStatus::PARALYSIS ? speed / 4 : speed;
}

bool outspeeds_all(const SimBattler& own, const SimBattler& foe, const std::vector<SimBattler>& to_come){
    const uint32_t speed = staged_speed(own);
    if (speed <= staged_speed(foe)){
        return false;
    }
    return std::ranges::all_of(to_come, [&](const SimBattler& next){
        return next.hp == 0 || speed > staged_speed(next);
    });
}

uint16_t eighth(const SimBattler& battler){
    return std::max<uint16_t>(battler.body.stats.hp / 8, 1);
}

//  D: the foe's highest roll, and the poison and Leech Seed on the lead.
uint32_t worst_turn(const SimBattler& own, const SimBattler& foe){
    uint32_t residual = own.status == SimStatus::POISON ? eighth(own) : 0;
    residual += own.seeded ? eighth(own) : 0;
    return foe_max_hit(foe, own) + residual;
}

//  The chance the foe's next turn is a move that only puts a status on
//  the lead (sleep, poison, paralysis), while it has none.
double status_move_chance(const SimBattler& own, const SimBattler& foe, const SimTrainer& trainer){
    if (own.status != SimStatus::NONE){
        return 0;
    }
    const std::vector<double> chances = ai_move_chances(trainer, foe, own);
    double chance = 0;
    for (size_t slot = 0; slot < foe.moves.size(); slot++){
        const MoveData* move = move_data(foe.moves[slot].move);
        const bool status = move != nullptr && move->power == 0 && (
            move->effect == "sleep" || move->effect == "poison" || move->effect == "toxic" || move->effect == "paralyze"
        );
        chance += status ? chances[slot] : 0;
    }
    return chance;
}

bool foe_explodes(const SimBattler& foe){
    return std::ranges::any_of(foe.moves, [](const SimMove& move){
        return move.pp > 0 && (move.move == "self-destruct" || move.move == "explosion");
    });
}

//  The accuracy stage to set up to: the goal over the foe's evasion, none
//  without a goal (a Minimize every turn outpaces X Accuracy).
int accuracy_goal(const SimBattler& foe, const SetupGoals& goals){
    if (goals.accuracy <= 0){
        return 0;
    }
    return std::min(goals.accuracy + std::max(stage_of(foe, Stage::EVASION), 0), MAX_STAGE);
}

//  N: the setup turns the goals still need, as far as the bag holds items.
int setup_turns_left(const SimBattler& own, const SimItems& items, const SimBattler& foe, const SetupGoals& goals){
    int turns = std::min(std::max(goals.attack - stage_of(own, Stage::ATTACK), 0), items.x_attacks);
    turns += std::min(std::max(goals.sp_attack - stage_of(own, Stage::SP_ATTACK), 0), items.x_specials);
    turns += std::min(std::max(accuracy_goal(foe, goals) - stage_of(own, Stage::ACCURACY), 0), items.x_accuracies);
    return turns;
}

//  The heal before a setup turn: the smallest whose HP after the foe's mean
//  turn reaches `reserve`, else the one that leaves the most, each above
//  `worst`. None if no heal gets above it.
std::optional<PlanItem> setup_heal(const SimBattler& own, const SimItems& items, uint32_t worst, double mean, double reserve){
    std::optional<PlanItem> best;
    double best_left = own.hp;
    for (PlanItem heal : HEALS){
        const uint16_t gain = plan_item_heal(heal, own);
        if (plan_item_count(items, heal) <= 0 || gain == 0 || own.hp + gain <= worst){
            continue;
        }
        const double left = own.hp + gain - mean;
        if (left >= reserve){
            return heal;
        }
        if (left > best_left){
            best = heal;
            best_left = left;
        }
    }
    return best;
}

//  The setup turn, or the heal before it; none to leave it to own_turn().
std::optional<PlanItem> setup_turn(
    const SimBattler& own, const SimItems& items, const SimBattler& foe,
    const SimTrainer& trainer, size_t foes_left,
    const std::vector<SimBattler>& to_come, const SetupGoals& goals
){
    const std::optional<PlanItem> next = next_setup_item(own, items, foe, to_come, goals);
    if (!next || own.status == SimStatus::SLEEP || foe_explodes(foe)){
        return std::nullopt;
    }
    if (status_move_chance(own, foe, trainer) > MAX_SETUP_STATUS_CHANCE){
        return std::nullopt;
    }
    const uint32_t worst = worst_turn(own, foe);
    if (worst >= own.body.stats.hp){
        return std::nullopt;
    }
    const PotionCase c = potion_case(own, foe, trainer);
    if (foes_left <= 1 && c.turns_to_ko <= 1){
        return std::nullopt;
    }
    const int pending = std::max(setup_turns_left(own, items, foe, goals) + (*next == PlanItem::X_SPEED ? 1 : 0), 1);
    const double reserve = std::min(0.75 * own.body.stats.hp, worst + (pending - 1) * c.expected_hit);
    if (own.hp > worst && own.hp >= reserve){
        return next;
    }
    return setup_heal(own, items, worst, c.expected_hit, reserve);
}

//  A heal bigger than a Potion, when the foe's next turn could faint the
//  lead and the lead doesn't faint it first: the smallest that pays.
std::optional<PlanItem> bigger_heal(const SimBattler& own, const SimItems& items, const SimBattler& foe, const SimTrainer& trainer){
    if (own.hp >= own.body.stats.hp || own.status == SimStatus::SLEEP){
        return std::nullopt;
    }
    const PotionCase c = potion_case(own, foe, trainer);
    if (c.max_hit < c.hp || (c.turns_to_ko <= 1 && staged_speed(own) > staged_speed(foe))){
        return std::nullopt;
    }
    for (PlanItem heal : HEALS){
        const uint16_t gain = plan_item_heal(heal, own);
        if (plan_item_count(items, heal) > 0 && gain > POTION_HP && potion_pays(c, gain)){
            return heal;
        }
    }
    return std::nullopt;
}

} // namespace


SetupGoals setup_goals(std::string_view trainer){
    const GoalTable& table = goal_table();
    const auto it = table.find(trainer);
    return it == table.end() ? SetupGoals{} : it->second;
}

std::string_view plan_item_slug(PlanItem item){
    switch (item){
    case PlanItem::SUPER_POTION:
        return "super-potion";
    case PlanItem::HYPER_POTION:
        return "hyper-potion";
    case PlanItem::FULL_RESTORE:
        return "full-restore";
    case PlanItem::X_ATTACK:
        return "x-attack";
    case PlanItem::X_SPECIAL:
        return "x-special";
    case PlanItem::X_SPEED:
        return "x-speed";
    case PlanItem::X_ACCURACY:
        return "x-accuracy";
    }
    return "";
}

uint16_t plan_item_heal(PlanItem item, const SimBattler& own){
    const uint16_t missing = static_cast<uint16_t>(own.body.stats.hp - std::min(own.hp, own.body.stats.hp));
    switch (item){
    case PlanItem::SUPER_POTION:
        return std::min(SUPER_POTION_HP, missing);
    case PlanItem::HYPER_POTION:
        return std::min(HYPER_POTION_HP, missing);
    case PlanItem::FULL_RESTORE:
        return missing;
    default:
        return 0;
    }
}

int plan_item_count(const SimItems& items, PlanItem item){
    switch (item){
    case PlanItem::SUPER_POTION:
        return items.super_potions;
    case PlanItem::HYPER_POTION:
        return items.hyper_potions;
    case PlanItem::FULL_RESTORE:
        return items.full_restores;
    case PlanItem::X_ATTACK:
        return items.x_attacks;
    case PlanItem::X_SPECIAL:
        return items.x_specials;
    case PlanItem::X_SPEED:
        return items.x_speeds;
    case PlanItem::X_ACCURACY:
        return items.x_accuracies;
    }
    return 0;
}

std::optional<PlanItem> next_setup_item(
    const SimBattler& own, const SimItems& items, const SimBattler& foe,
    const std::vector<SimBattler>& to_come, const SetupGoals& goals
){
    if (stage_of(own, Stage::ATTACK) < goals.attack && items.x_attacks > 0){
        return PlanItem::X_ATTACK;
    }
    if (stage_of(own, Stage::SP_ATTACK) < goals.sp_attack && items.x_specials > 0){
        return PlanItem::X_SPECIAL;
    }
    if (goals.outspeed && items.x_speeds > 0 && stage_of(own, Stage::SPEED) < MAX_STAGE && !outspeeds_all(own, foe, to_come)){
        return PlanItem::X_SPEED;
    }
    if (stage_of(own, Stage::ACCURACY) < accuracy_goal(foe, goals) && items.x_accuracies > 0){
        return PlanItem::X_ACCURACY;
    }
    return std::nullopt;
}

PlannedTurn planned_turn(
    const SimBattler& own, const SimItems& items, const SimBattler& foe,
    const SimTrainer& trainer, size_t foes_left,
    const std::vector<SimBattler>& to_come, const SetupGoals& goals
){
    if (goals.any()){
        if (const std::optional<PlanItem> setup = setup_turn(own, items, foe, trainer, foes_left, to_come, goals)){
            return PlannedTurn{.item = setup};
        }
    }
    const OwnTurn turn = own_turn(own, items, foe, trainer, foes_left, to_come);
    if (turn.potion || turn.awakening){
        return PlannedTurn{.turn = turn};
    }
    if (const std::optional<PlanItem> heal = bigger_heal(own, items, foe, trainer)){
        return PlannedTurn{.item = heal};
    }
    return PlannedTurn{.turn = turn};
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
