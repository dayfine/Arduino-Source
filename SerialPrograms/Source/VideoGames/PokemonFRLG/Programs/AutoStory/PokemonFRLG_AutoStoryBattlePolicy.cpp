/*  AutoStory Battle Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Thunder Badge: Speed x1.1. Every fight a policy is written for is after Lt. Surge.
constexpr double THUNDER_BADGE = 1.1;
//  Guard Spec's Mist lasts this many turns, the one it is used on included.
constexpr int MIST_TURNS = 5;
constexpr int MAX_STAGE = 6;
constexpr double WHOLE_HP = std::numeric_limits<double>::infinity();

//  The HP medicines, smallest first (the doc's HEALS table).
struct Medicine{
    std::string_view item;
    double hp;
};
constexpr Medicine MEDICINES[] = {
    {"potion", 20}, {"super-potion", 50}, {"hyper-potion", 200}, {"max-potion", WHOLE_HP}, {"full-restore", WHOLE_HP},
};

//  The cure for a status, by the doc's CURES table; "" for none.
std::string_view cure_of(StatusCondition status){
    switch (status){
    case StatusCondition::SLEEP:
        return "awakening";
    case StatusCondition::POISON:
        return "antidote";
    case StatusCondition::PARALYSIS:
        return "parlyz-heal";
    case StatusCondition::NONE:
    case StatusCondition::OTHER:
        return "";
    }
    return "";
}

double stage_multiplier(int stage){
    return stage >= 0 ? (2.0 + stage) / 2.0 : 2.0 / (2.0 - stage);
}

PolicyItem medicine_item(const std::string& item, std::string why){
    return PolicyItem{.item = item, .party_member = true, .why = std::move(why)};
}

PolicyItem setup_item_of(const std::string& item, std::string why){
    return PolicyItem{.item = item, .party_member = false, .why = std::move(why)};
}

} // namespace


std::optional<BattlePolicyKind> battle_policy_kind(std::string_view word){
    if (word == "solo-recovery"){
        return BattlePolicyKind::SOLO_RECOVERY;
    }
    if (word == "goal-based-setup"){
        return BattlePolicyKind::GOAL_BASED_SETUP;
    }
    return std::nullopt;
}

std::optional<std::vector<FoeThreat>> foe_threats_of(const std::vector<std::vector<int64_t>>& rows){
    constexpr size_t COLUMNS = 7;
    std::vector<FoeThreat> foes;
    for (const std::vector<int64_t>& row : rows){
        if (row.size() != COLUMNS){
            return std::nullopt;
        }
        foes.push_back(FoeThreat{
            .speed = static_cast<int>(row[0]),
            .likely = static_cast<int>(row[1]),
            .mean = static_cast<int>(row[2]),
            .max = static_cast<int>(row[3]),
            .normal_max = static_cast<int>(row[4]),
            .lowers_accuracy = row[5] != 0,
            .explodes = row[6] != 0,
        });
    }
    return foes;
}

BattlePolicy battle_policy_of(const Pokemon::StoryActionArgs& args){
    const std::optional<BattlePolicyKind> kind = battle_policy_kind(Pokemon::story_arg<std::string>(args, "policy"));
    if (!kind){
        return {};
    }
    BattlePolicy policy;
    policy.kind = *kind;
    policy.goals = SetupGoals{
        .spa = static_cast<int>(Pokemon::story_arg<int64_t>(args, "setup_spa")),
        .accuracy = static_cast<int>(Pokemon::story_arg<int64_t>(args, "setup_accuracy")),
        .outspeed = Pokemon::story_arg<bool>(args, "setup_outspeed"),
        .guard_accuracy = Pokemon::story_arg<bool>(args, "setup_guard_accuracy"),
        .torrent_power = Pokemon::story_arg<bool>(args, "setup_torrent_power"),
    };
    if (std::optional<std::vector<FoeThreat>> foes = foe_threats_of(Pokemon::story_arg<std::vector<std::vector<int64_t>>>(args, "foe_threats"))){
        policy.foes = std::move(*foes);
    }
    return policy;
}

void end_trainer_battle(Supplies& supplies){
    supplies.foes.clear();
    supplies.trainer.clear();
    supplies.foe_out.clear();
    supplies.battle_items.clear();
    supplies.policy = {};
}

int bag_count(const Supplies& supplies, std::string_view item){
    const std::string name(item);
    const auto other = supplies.other_counts.find(name);
    return other != supplies.other_counts.end() ? other->second : item_count(supplies, name);
}

double medicine_hp(std::string_view item){
    for (const Medicine& medicine : MEDICINES){
        if (medicine.item == item){
            return medicine.hp;
        }
    }
    return 0;
}


PolicyState::PolicyState(const BattlePolicy& policy)
    : m_policy(policy)
{}

const FoeThreat* PolicyState::foe(const PolicyView& view) const{
    return view.foe_index < m_policy.foes.size() ? &m_policy.foes[view.foe_index] : nullptr;
}

PolicyState::Danger PolicyState::danger(const PolicyView& view) const{
    //  A poisoned lead also loses 1/8 of its HP to the poison each turn.
    const double residual = view.status == StatusCondition::POISON ? std::floor(view.max_hp / 8) : 0;
    const FoeThreat* threat = foe(view);
    if (threat == nullptr){
        return {residual, residual, residual, residual};
    }
    return {
        residual + threat->likely, residual + threat->mean, residual + threat->max, residual + threat->normal_max,
    };
}

int PolicyState::own_speed(const PolicyView& view) const{
    const int badged = static_cast<int>(std::floor(view.speed * THUNDER_BADGE));
    return static_cast<int>(std::floor(badged * stage_multiplier(m_speed)));
}

bool PolicyState::outsped(const PolicyView& view) const{
    const int own = own_speed(view);
    for (size_t index = view.foe_index; index < m_policy.foes.size(); index++){
        if (m_policy.foes[index].speed >= own){
            return false;
        }
    }
    return true;
}

int PolicyState::spa_goal(const PolicyView& view) const{
    const int target = m_policy.goals.spa;
    const bool torrent = view.hp * 3 <= view.max_hp;
    if (!m_policy.goals.torrent_power || torrent){
        return target;
    }
    //  The stage whose multiplier makes up for Torrent's 1.5.
    const double wanted = 1.5 * (2.0 + target) / 2.0;
    for (int stage = target; stage <= MAX_STAGE; stage++){
        if ((2.0 + stage) / 2.0 >= wanted){
            return stage;
        }
    }
    return MAX_STAGE;
}

int PolicyState::accuracy_goal() const{
    return std::min(MAX_STAGE, m_policy.goals.accuracy + std::max(0, m_foe_evasion));
}

std::optional<PolicyItem> PolicyState::cure(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    if (view.hp <= danger.likely){
        return std::nullopt;
    }
    const std::string_view own_cure = cure_of(view.status);
    std::vector<std::string_view> candidates;
    if (!own_cure.empty()){
        candidates = {own_cure, "full-heal", "full-restore"};
    }else if (view.status == StatusCondition::OTHER){
        candidates = {"full-heal", "full-restore"};
    }
    for (const std::string_view item : candidates){
        if (bag_count(supplies, item) > 0){
            return medicine_item(std::string(item), std::format("{} for {}", item, status_name(view.status)));
        }
    }
    return std::nullopt;
}

std::optional<PolicyItem> PolicyState::heal(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    if (view.hp > std::max(view.max_hp / 2, danger.max)){
        return std::nullopt;
    }
    const double missing = view.max_hp - view.hp;
    for (const Medicine& medicine : MEDICINES){
        const double gained = std::min(medicine.hp, missing);
        if (bag_count(supplies, medicine.item) > 0 && gained > danger.likely && gained > 0){
            return medicine_item(std::string(medicine.item), std::format(
                "{}: HP {:.0f}/{:.0f}, a turn's loss up to {:.0f}", medicine.item, view.hp, view.max_hp, danger.max
            ));
        }
    }
    return std::nullopt;
}

int PolicyState::setup_turns(const Supplies& supplies, const PolicyView& view, bool guard) const{
    int count = std::min(bag_count(supplies, "x-special"), std::max(0, spa_goal(view) - m_spa));
    count += std::min(bag_count(supplies, "x-accuracy"), std::max(0, accuracy_goal() - m_accuracy));
    if (m_policy.goals.outspeed && !outsped(view) && bag_count(supplies, "x-speed") > 0){
        count++;
    }
    if (guard){
        count++;
    }
    return std::max(1, count);
}

std::optional<PolicyItem> PolicyState::setup(const Supplies& supplies, const PolicyView& view) const{
    const bool special = m_spa < spa_goal(view) && bag_count(supplies, "x-special") > 0;
    const bool speed = m_policy.goals.outspeed && bag_count(supplies, "x-speed") > 0
        && m_speed < MAX_STAGE && !outsped(view);
    const bool accuracy = m_accuracy < accuracy_goal() && bag_count(supplies, "x-accuracy") > 0;
    if (!special && !speed && !accuracy){
        return std::nullopt;
    }
    const FoeThreat* threat = foe(view);
    if (m_policy.goals.guard_accuracy && bag_count(supplies, "guard-spec") > 0 && m_mist_turns <= 0
        && threat != nullptr && threat->lowers_accuracy
    ){
        return setup_item_of("guard-spec", "Guard Spec: the foe lowers accuracy and setup is pending");
    }
    if (special){
        return setup_item_of("x-special", std::format("X Special: Sp. Atk +{} of +{}", m_spa, spa_goal(view)));
    }
    if (speed){
        return setup_item_of("x-speed", std::format("X Speed: Speed {} against the foes left", own_speed(view)));
    }
    return setup_item_of("x-accuracy", std::format("X Accuracy: accuracy {:+} of {:+}", m_accuracy, accuracy_goal()));
}

std::optional<PolicyItem> PolicyState::planning_heal(
    const Supplies& supplies, const PolicyView& view, const Danger& danger, int turns, std::string_view waiting_on
) const{
    //  An HP reserve for the setup turns ahead.
    const double reserve = std::min(view.max_hp * 0.75, danger.normal_max + std::max(0, turns - 1) * danger.mean);
    if (view.hp > danger.normal_max && view.hp >= reserve){
        return std::nullopt;
    }
    struct Option{
        bool sufficient;
        double expected;
        double restored;
        std::string_view item;
    };
    std::vector<Option> options;
    for (const Medicine& medicine : MEDICINES){
        const double restored = std::min(view.max_hp - view.hp, medicine.hp);
        const double after = view.hp + restored;
        if (bag_count(supplies, medicine.item) <= 0 || restored <= 0 || after <= danger.normal_max){
            continue;
        }
        //  A heal that the foe's mean hit and the poison take back, and more,
        //  is futile (the 2026-11-12 loss to Koga: Potions of 20 against 42 a
        //  turn, four turns running, never attacking): the doc's "if HP still
        //  cannot exceed D, attack". Only a heal that gains ground is a heal.
        const double expected = after - danger.mean;
        if (expected <= view.hp){
            continue;
        }
        options.push_back({expected >= reserve, expected, restored, medicine.item});
    }
    const Option* chosen = nullptr;
    for (const Option& option : options){
        if (option.sufficient && (chosen == nullptr || option.restored < chosen->restored)){
            chosen = &option;
        }
    }
    if (chosen == nullptr){
        for (const Option& option : options){
            if (chosen == nullptr || option.expected > chosen->expected){
                chosen = &option;
            }
        }
    }
    if (chosen == nullptr){
        return std::nullopt;
    }
    return medicine_item(std::string(chosen->item), std::format(
        "{}: HP {:.0f}/{:.0f} against a hit of {:.0f}, {} setup turn(s) ahead ({})",
        chosen->item, view.hp, view.max_hp, danger.normal_max, turns, waiting_on
    ));
}

std::optional<PolicyItem> PolicyState::choose(const Supplies& supplies, const PolicyView& view){
    if (m_mist_turns > 0){
        m_mist_turns--;
    }
    if (m_policy.kind == BattlePolicyKind::NONE){
        return std::nullopt;
    }
    const Danger risk = danger(view);
    if (std::optional<PolicyItem> item = cure(supplies, view, risk)){
        return item;
    }
    const bool setting_up = m_policy.kind == BattlePolicyKind::GOAL_BASED_SETUP && view.status != StatusCondition::SLEEP;
    if (!setting_up){
        return heal(supplies, view, risk);
    }
    const std::optional<PolicyItem> planned = setup(supplies, view);
    const int turns = planned ? setup_turns(supplies, view, planned->item == "guard-spec") : 1;
    //  With no setup pending the one turn is the attack's own, not a setup's.
    const std::string waiting_on = planned ? std::format("waiting on {}", planned->item) : std::string("no setup pending");
    if (std::optional<PolicyItem> item = planning_heal(supplies, view, risk, turns, waiting_on)){
        return item;
    }
    if (view.hp <= risk.normal_max){
        return std::nullopt;
    }
    return planned;
}

void PolicyState::item_used(const std::string& item){
    if (item == "x-special"){
        m_spa = std::min(MAX_STAGE, m_spa + 1);
    }else if (item == "x-speed"){
        m_speed = std::min(MAX_STAGE, m_speed + 1);
    }else if (item == "x-accuracy"){
        m_accuracy = std::min(MAX_STAGE, m_accuracy + 1);
    }else if (item == "guard-spec"){
        m_mist_turns = MIST_TURNS;
    }
}

void PolicyState::foe_evasion_raised(int stages){
    m_foe_evasion = std::clamp(m_foe_evasion + stages, -MAX_STAGE, MAX_STAGE);
}

void PolicyState::own_accuracy_lowered(int stages){
    m_accuracy = std::clamp(m_accuracy - stages, -MAX_STAGE, MAX_STAGE);
}

void PolicyState::own_speed_lowered(int stages){
    m_speed = std::clamp(m_speed - stages, -MAX_STAGE, MAX_STAGE);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
