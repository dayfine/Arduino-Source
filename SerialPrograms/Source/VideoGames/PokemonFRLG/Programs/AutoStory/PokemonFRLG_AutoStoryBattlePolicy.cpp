/*  AutoStory Battle Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryBattleRisk.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryParty.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Thunder Badge: Speed x1.1. Every fight a policy is written for is after Lt. Surge.
constexpr double THUNDER_BADGE = 1.1;
//  Guard Spec's Mist lasts this many turns, the one it is used on included.
constexpr int MIST_TURNS = 5;
//  Sunny Day's sun lasts five turns, the one it is used in included: our
//  next four choices.
constexpr int SUN_TURNS = 4;
constexpr int MAX_STAGE = 6;
constexpr double WHOLE_HP = std::numeric_limits<double>::infinity();
//  Gen 3's lowest damage roll: 85 of 100.
constexpr double LOWEST_ROLL = 0.85;

//  The HP medicines, smallest first (the doc's HEALS table).
struct Medicine{
    std::string_view item;
    double hp;
};
constexpr Medicine MEDICINES[] = {
    {"potion", 20}, {"super-potion", 50}, {"hyper-potion", 200}, {"max-potion", WHOLE_HP}, {"full-restore", WHOLE_HP},
};

//  A Full Restore (a heal and a cure in one) is used only when it also heals
//  at least half of max HP; below that a cheap cure or a smaller medicine does.
bool full_restore_worth_it(const PolicyView& view){
    return view.max_hp - view.hp >= view.max_hp / 2;
}

//  The battle HP bar's colour.
enum class HpBand{
    GREEN,
    YELLOW,
    RED,
};

HpBand hp_band(const PolicyView& view){
    if (view.hp * 5 <= view.max_hp){
        return HpBand::RED;
    }
    if (view.hp * 2 <= view.max_hp){
        return HpBand::YELLOW;
    }
    return HpBand::GREEN;
}

//  The heal by the fight's risk and the lead's HP band: the first of
//  `items` in the bag. A row with none leaves it to the HP-against-the-hit
//  rules (heal(), planning_heal()), which take the smallest medicine that
//  clears the foe's hit: Potions and Super Potions. Against the Elite Four
//  and the Champion that left Blastoise in the red for many turns and
//  fainted it with 13 Full Restores in the bag (console B, 2026-11-16,
//  Lorelei).
struct RiskHeal{
    BattleRisk risk;
    HpBand band;
    std::array<std::string_view, 3> items;
};
constexpr RiskHeal RISK_HEALS[] = {
    {BattleRisk::LOW, HpBand::GREEN, {}},
    {BattleRisk::LOW, HpBand::YELLOW, {}},
    {BattleRisk::LOW, HpBand::RED, {}},
    {BattleRisk::HIGH, HpBand::GREEN, {}},
    {BattleRisk::HIGH, HpBand::YELLOW, {"full-restore", "max-potion", "hyper-potion"}},
    {BattleRisk::HIGH, HpBand::RED, {"full-restore", "max-potion", "hyper-potion"}},
};

PolicyItem medicine_item(const std::string& item, std::string why){
    return PolicyItem{.item = item, .party_member = true, .why = std::move(why)};
}

PolicyItem setup_item_of(const std::string& item, std::string why){
    return PolicyItem{.item = item, .party_member = false, .why = std::move(why)};
}

std::string_view band_name(HpBand band){
    switch (band){
    case HpBand::GREEN:
        return "green";
    case HpBand::YELLOW:
        return "yellow";
    case HpBand::RED:
        return "red";
    }
    return "";
}

//  RISK_HEALS' item for the fight and the lead's HP, if the row names one in the bag.
std::optional<PolicyItem> risk_heal(const Supplies& supplies, const PolicyView& view){
    const HpBand band = hp_band(view);
    for (const RiskHeal& row : RISK_HEALS){
        if (row.risk != supplies.risk || row.band != band){
            continue;
        }
        for (const std::string_view item : row.items){
            if (!item.empty() && bag_count(supplies, item) > 0){
                return medicine_item(std::string(item), std::format(
                    "{}: HP {:.0f}/{:.0f} ({}) in a {}-risk fight",
                    item, view.hp, view.max_hp, band_name(band), battle_risk_name(supplies.risk)
                ));
            }
        }
    }
    return std::nullopt;
}

} // namespace


std::optional<BattlePolicyKind> battle_policy_kind(std::string_view word){
    if (word == "solo-recovery"){
        return BattlePolicyKind::SOLO_RECOVERY;
    }
    if (word == "goal-based-setup"){
        return BattlePolicyKind::GOAL_BASED_SETUP;
    }
    if (word == "budgeted-setup"){
        return BattlePolicyKind::BUDGETED_SETUP;
    }
    return std::nullopt;
}

std::optional<std::vector<FoeThreat>> foe_threats_of(const std::vector<std::vector<int64_t>>& rows){
    constexpr size_t COLUMNS = 7;
    constexpr size_t WITH_PHAZES = 8;
    constexpr size_t WITH_CHARGES = 9;
    constexpr size_t WITH_PRIORITY = 10;
    std::vector<FoeThreat> foes;
    for (const std::vector<int64_t>& row : rows){
        if (row.size() < COLUMNS || row.size() > WITH_PRIORITY){
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
            .phazes = row.size() >= WITH_PHAZES && row[7] != 0,
            .charges = row.size() >= WITH_CHARGES && row[8] != 0,
            .priority = row.size() == WITH_PRIORITY && row[9] != 0,
        });
    }
    return foes;
}

namespace{

//  A battle step's "foe_stats" rows, [defense, sp_defense] or [defense,
//  sp_defense, hp] each; empty if there are none or a row isn't that.
std::vector<FoeStats> foe_stats_of(const Pokemon::StoryActionArgs& args){
    constexpr size_t COLUMNS = 2;
    constexpr size_t WITH_HP = 3;
    std::vector<FoeStats> foes;
    for (const std::vector<int64_t>& row : Pokemon::story_arg<std::vector<std::vector<int64_t>>>(args, "foe_stats")){
        if (row.size() != COLUMNS && row.size() != WITH_HP){
            return {};
        }
        FoeStats stats{.defense = static_cast<int>(row[0]), .sp_defense = static_cast<int>(row[1])};
        if (row.size() == WITH_HP){
            stats.hp = static_cast<int>(row[2]);
        }
        foes.push_back(stats);
    }
    return foes;
}

//  The lead's best attack, at Sp. Atk `spa_stage` and its lowest roll,
//  KOs the foe out from `fill` of its HP (1: full): the plan has the foe's HP.
bool attack_kos(const BattlePolicy& policy, const Supplies& supplies, const PolicyView& view, int spa_stage, double fill = 1.0){
    if (view.foe_index >= policy.foe_stats.size() || view.foe_index >= supplies.foes.size()){
        return false;
    }
    const FoeStats& stats = policy.foe_stats[view.foe_index];
    if (!stats.hp){
        return false;
    }
    const AttackContext context{.sp_attack_stage = spa_stage, .foe = stats};
    return most_damage_against(supplies.lead, supplies.foes[view.foe_index], context) * LOWEST_ROLL >= *stats.hp * fill;
}

//  The bag holds `item` and the battle's budget, if it has one, has some left.
bool within_budget(const Supplies& supplies, std::string_view item){
    if (bag_count(supplies, item) <= 0){
        return false;
    }
    if (!supplies.budget){
        return true;
    }
    return budget_left(*supplies.budget, item).value_or(1) > 0;
}

//  The lead's status's own cure, else a Full Heal, from the bag; none
//  asleep (it attacks) or without a status.
std::optional<PolicyItem> status_cure(const Supplies& supplies, const PolicyView& view){
    if (view.status == StatusCondition::NONE || view.status == StatusCondition::SLEEP){
        return std::nullopt;
    }
    for (const std::string_view item : {cure_of(view.status), std::string_view("full-heal")}){
        if (!item.empty() && bag_count(supplies, item) > 0){
            return medicine_item(std::string(item), std::format("{} for {}", item, status_name(view.status)));
        }
    }
    return std::nullopt;
}

//  The lead knows Surf and has no PP left for it.
bool surf_empty(const PartyMember& lead){
    for (const KnownMove& move : lead.moves){
        if (move.move == "surf"){
            return move.pp == 0;
        }
    }
    return false;
}

} // namespace

BattlePolicy battle_policy_of(const Pokemon::StoryActionArgs& args){
    const std::optional<BattlePolicyKind> kind = battle_policy_kind(Pokemon::story_arg<std::string>(args, "policy"));
    BattlePolicy policy;
    policy.foe_stats = foe_stats_of(args);
    if (!kind){
        return policy;
    }
    policy.kind = *kind;
    policy.goals = SetupGoals{
        .spa = static_cast<int>(Pokemon::story_arg<int64_t>(args, "setup_spa")),
        .accuracy = static_cast<int>(Pokemon::story_arg<int64_t>(args, "setup_accuracy")),
        .outspeed = Pokemon::story_arg<bool>(args, "setup_outspeed"),
        .guard_accuracy = Pokemon::story_arg<bool>(args, "setup_guard_accuracy"),
        .torrent_power = Pokemon::story_arg<bool>(args, "setup_torrent_power"),
        .target_torrent = Pokemon::story_arg<bool>(args, "setup_target_torrent"),
    };
    if (std::optional<std::vector<FoeThreat>> foes = foe_threats_of(Pokemon::story_arg<std::vector<std::vector<int64_t>>>(args, "foe_threats"))){
        policy.foes = std::move(*foes);
    }
    return policy;
}

void end_trainer_battle(Supplies& supplies){
    supplies.budget.reset();
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
    if (threat == nullptr || (threat->charges && !m_foe_charging && m_sun_turns <= 0)){
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
    const bool opening = view.foe_index == 0 && m_policy.foes.size() > 1;
    const bool make_up = m_policy.goals.torrent_power || (m_policy.goals.target_torrent && opening);
    if (!make_up || torrent){
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
        candidates = {own_cure, "full-heal"};
    }else if (view.status == StatusCondition::OTHER){
        candidates = {"full-heal"};
    }
    if (!candidates.empty() && full_restore_worth_it(view)){
        candidates.push_back("full-restore");
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
        if (medicine.item == "full-restore" && !full_restore_worth_it(view)){
            continue;
        }
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
    //  Setup pays only while its stages last and the foe out needs more
    //  than one attack. Bruno's first Onix (console B, 2026-11-16, 20.02):
    //  four X Specials towards the Torrent stall's +4 while Surf KOs it at
    //  +0, then its Roar sent Blastoise out with them.
    const FoeThreat* threat = foe(view);
    if ((threat != nullptr && threat->phazes) || attack_kos(m_policy, supplies, view, m_spa)){
        return std::nullopt;
    }
    const bool special = m_spa < spa_goal(view) && bag_count(supplies, "x-special") > 0;
    const bool speed = m_policy.goals.outspeed && bag_count(supplies, "x-speed") > 0
        && m_speed < MAX_STAGE && !outsped(view);
    const bool accuracy = m_accuracy < accuracy_goal() && bag_count(supplies, "x-accuracy") > 0;
    if (!special && !speed && !accuracy){
        return std::nullopt;
    }
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
        //  A heal that the mean hit and poison take back is futile (Koga,
        //  2026-11-12: Potions of 20 against 42 a turn): attack instead.
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

PolicyState::Turn PolicyState::choose(const Supplies& supplies, const PolicyView& view){
    if (m_mist_turns > 0){
        m_mist_turns--;
    }
    m_attack_reason.clear();
    for (const std::string& letters : view.battle_texts){
        note_battle_text(letters);
    }
    Turn turn = m_policy.kind == BattlePolicyKind::BUDGETED_SETUP
        ? choose_budgeted(supplies, view)
        : Turn{.item = choose_unbudgeted(supplies, view)};
    //  The charged hit goes off this turn; the next charge has its own text.
    m_foe_charging = false;
    if (m_sun_turns > 0){
        m_sun_turns--;
    }
    return turn;
}

std::optional<PolicyItem> PolicyState::choose_unbudgeted(const Supplies& supplies, const PolicyView& view){
    if (m_policy.kind == BattlePolicyKind::NONE){
        return std::nullopt;
    }
    //  A whole heal also cures, so it goes before a status's own cure.
    if (std::optional<PolicyItem> item = risk_heal(supplies, view)){
        return item;
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

bool PolicyState::sure_ko_first(const Supplies& supplies, const PolicyView& view) const{
    const FoeThreat* threat = foe(view);
    if (threat == nullptr || view.status == StatusCondition::SLEEP){
        return false;
    }
    //  Paralysis quarters the lead's Speed.
    const int speed = view.status == StatusCondition::PARALYSIS ? own_speed(view) / 4 : own_speed(view);
    return speed > threat->speed && attack_kos(m_policy, supplies, view, m_spa, view.foe_fill);
}

bool PolicyState::strength_kos(const Supplies& supplies, const PolicyView& view) const{
    if (view.foe_index >= m_policy.foe_stats.size() || view.foe_index >= supplies.foes.size()){
        return false;
    }
    const FoeStats& stats = m_policy.foe_stats[view.foe_index];
    if (!stats.hp){
        return false;
    }
    //  The lead with Strength alone, for its damage.
    PartyMember strength_only = supplies.lead;
    std::erase_if(strength_only.moves, [](const KnownMove& move){
        return move.move != "strength";
    });
    const AttackContext context{.sp_attack_stage = m_spa, .foe = stats};
    const double lowest = most_damage_against(strength_only, supplies.foes[view.foe_index], context) * LOWEST_ROLL;
    return lowest > 0 && lowest >= *stats.hp * view.foe_fill;
}

std::optional<PolicyItem> PolicyState::ko_risk_heal(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    //  A heal that gains less than the foe's best hit plus its mean hit
    //  only buys back the next hit: the bigger heal goes (Lorelei's
    //  Lapras, 2026-11-16: Super Potions of 50 against hits up to 39).
    for (const Medicine& medicine : MEDICINES){
        if (std::isinf(medicine.hp) && medicine.item != "max-potion"){
            continue;
        }
        //  Potions and Super Potions have no budget; Hyper and Max Potions do.
        if (!within_budget(supplies, medicine.item)){
            continue;
        }
        const double gained = std::min(view.max_hp - view.hp, medicine.hp);
        if (view.hp + gained > danger.normal_max && gained >= danger.normal_max + danger.mean){
            return medicine_item(std::string(medicine.item), std::format(
                "{}: KO risk, HP {:.0f} <= the foe's best hit {:.0f}", medicine.item, view.hp, danger.normal_max
            ));
        }
    }
    for (const std::string_view item : {std::string_view("full-restore"), std::string_view("max-potion"), std::string_view("hyper-potion")}){
        if (bag_count(supplies, item) > 0){
            const bool over = supplies.budget && budget_left(*supplies.budget, item).value_or(1) <= 0;
            return medicine_item(std::string(item), std::format(
                "{}: KO risk, HP {:.0f} <= the foe's best hit {:.0f}{}",
                item, view.hp, danger.normal_max, over ? " (over the fight's budget)" : ""
            ));
        }
    }
    return std::nullopt;
}

std::optional<PolicyItem> PolicyState::reserve_heal(
    const Supplies& supplies, const PolicyView& view, const Danger& danger, double reserve
) const{
    for (const Medicine& medicine : MEDICINES){
        if (!within_budget(supplies, medicine.item)){
            continue;
        }
        const double after = view.hp + std::min(view.max_hp - view.hp, medicine.hp);
        const bool whole = std::isinf(medicine.hp);
        //  A part heal must hold the reserve after the mean hit; a whole
        //  heal is the most there is.
        if ((whole && after - danger.mean > view.hp) || after - danger.mean >= reserve){
            return medicine_item(std::string(medicine.item), std::format(
                "{}: HP {:.0f} under the setup reserve {:.0f}", medicine.item, view.hp, reserve
            ));
        }
    }
    return std::nullopt;
}

std::optional<PolicyItem> PolicyState::budgeted_setup(const Supplies& supplies, const PolicyView& view) const{
    const FoeThreat* threat = foe(view);
    if ((threat != nullptr && threat->phazes) || attack_kos(m_policy, supplies, view, m_spa, view.foe_fill)){
        return std::nullopt;
    }
    if (m_policy.goals.guard_accuracy && within_budget(supplies, "guard-spec") && m_mist_turns <= 0
        && threat != nullptr && threat->lowers_accuracy
    ){
        return setup_item_of("guard-spec", "Guard Spec: the foe lowers accuracy");
    }
    if (m_spa < spa_goal(view) && within_budget(supplies, "x-special")){
        return setup_item_of("x-special", std::format("X Special: Sp. Atk +{} of +{}", m_spa, spa_goal(view)));
    }
    if (m_policy.goals.outspeed && m_speed < MAX_STAGE && !outsped(view) && within_budget(supplies, "x-speed")){
        return setup_item_of("x-speed", std::format("X Speed: Speed {} against the foes left", own_speed(view)));
    }
    //  The goal alone, not the foe's evasion: chasing Double Team with X
    //  Accuracy won less in the sim.
    if (m_accuracy < m_policy.goals.accuracy && within_budget(supplies, "x-accuracy")){
        return setup_item_of("x-accuracy", std::format("X Accuracy: accuracy {:+} of {:+}", m_accuracy, m_policy.goals.accuracy));
    }
    return std::nullopt;
}

PolicyState::Turn PolicyState::choose_budgeted(const Supplies& supplies, const PolicyView& view){
    using Row = std::optional<Choice> (PolicyState::*)(const Supplies&, const PolicyView&, const Danger&) const;
    static constexpr Row ROWS[] = {
        &PolicyState::row_sure_ko,
        &PolicyState::row_strength_finisher,
        &PolicyState::row_escalate,
        &PolicyState::row_status_at_ko_risk,
        &PolicyState::row_asleep,
        &PolicyState::row_status_cure,
        &PolicyState::row_ko_risk,
        &PolicyState::row_surf_empty,
        &PolicyState::row_setup,
    };
    if (view.foe_index != m_streak_foe || view.foe_fill != m_streak_fill){
        m_medicine_streak = 0;
    }
    const Danger risk = danger(view);
    Choice choice{.item = std::nullopt, .attack_reason = "attack"};
    for (const Row row : ROWS){
        if (std::optional<Choice> chosen = (this->*row)(supplies, view, risk)){
            choice = std::move(*chosen);
            break;
        }
    }
    if (choice.item && choice.item->party_member){
        m_medicine_streak++;
        m_streak_foe = view.foe_index;
        m_streak_fill = view.foe_fill;
        m_last_medicine = choice.item->item;
    }else{
        m_medicine_streak = 0;
    }
    if (!choice.item){
        m_attack_reason = std::move(choice.attack_reason);
    }
    return Turn{.item = std::move(choice.item), .move = std::move(choice.move)};
}

std::optional<PolicyState::Choice> PolicyState::row_sure_ko(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    if (!sure_ko_first(supplies, view)){
        return std::nullopt;
    }
    const FoeThreat* threat = foe(view);
    if (threat != nullptr && threat->priority && view.hp <= danger.normal_max){
        return std::nullopt;
    }
    if (strength_kos(supplies, view)){
        return Choice{.item = std::nullopt, .attack_reason = "attack: we move first and Strength KOs", .move = "strength"};
    }
    return Choice{.item = std::nullopt, .attack_reason = "attack: we move first and the hit KOs"};
}

std::optional<PolicyState::Choice> PolicyState::row_strength_finisher(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    if (view.status == StatusCondition::SLEEP || view.hp <= danger.normal_max || !strength_kos(supplies, view)){
        return std::nullopt;
    }
    return Choice{
        .item = std::nullopt,
        .attack_reason = std::format("attack: Strength KOs and the foe's best hit {:.0f} doesn't KO first", danger.normal_max),
        .move = "strength",
    };
}

std::optional<PolicyState::Choice> PolicyState::row_escalate(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    constexpr int MOST_MEDICINE_TURNS = 2;
    if (m_medicine_streak < MOST_MEDICINE_TURNS){
        return std::nullopt;
    }
    const bool whole_last = std::isinf(medicine_hp(m_last_medicine));
    if (view.hp <= danger.normal_max && danger.normal_max < view.max_hp && !whole_last){
        for (const std::string_view item : {std::string_view("full-restore"), std::string_view("max-potion")}){
            if (bag_count(supplies, item) > 0){
                return Choice{.item = medicine_item(std::string(item), std::format(
                    "{}: {} medicine turns and the foe's bar hasn't moved; KO risk, HP {:.0f} <= {:.0f}",
                    item, m_medicine_streak, view.hp, danger.normal_max
                ))};
            }
        }
    }
    return Choice{
        .item = std::nullopt,
        .attack_reason = std::format("attack: {} medicine turns and the foe's bar hasn't moved", m_medicine_streak),
    };
}

std::optional<PolicyState::Choice> PolicyState::row_status_at_ko_risk(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    if (view.status == StatusCondition::NONE || view.hp > danger.normal_max || danger.normal_max >= view.max_hp
        || bag_count(supplies, "full-restore") <= 0
    ){
        return std::nullopt;
    }
    return Choice{.item = medicine_item("full-restore", std::format(
        "full-restore: {} and KO risk, HP {:.0f} <= the foe's best hit {:.0f}", status_name(view.status), view.hp, danger.normal_max
    ))};
}

std::optional<PolicyState::Choice> PolicyState::row_asleep(const Supplies& supplies, const PolicyView& view, const Danger&) const{
    if (view.status != StatusCondition::SLEEP){
        return std::nullopt;
    }
    for (const std::string_view item : {cure_of(view.status), std::string_view("full-heal")}){
        if (!item.empty() && bag_count(supplies, item) > 0){
            return Choice{.item = medicine_item(std::string(item), std::format("{}: asleep", item))};
        }
    }
    return Choice{.item = std::nullopt, .attack_reason = "attack: asleep, no cure in the bag"};
}

std::optional<PolicyState::Choice> PolicyState::row_status_cure(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    if (view.hp <= danger.normal_max){
        return std::nullopt;
    }
    if (std::optional<PolicyItem> item = status_cure(supplies, view)){
        return Choice{.item = std::move(item)};
    }
    return std::nullopt;
}

std::optional<PolicyState::Choice> PolicyState::row_ko_risk(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    if (view.hp > danger.normal_max){
        return std::nullopt;
    }
    if (danger.normal_max >= view.max_hp){
        return Choice{.item = std::nullopt, .attack_reason = "attack: the foe's best hit KOs from full HP, no medicine helps"};
    }
    if (std::optional<PolicyItem> item = ko_risk_heal(supplies, view, danger)){
        return Choice{.item = std::move(item)};
    }
    return Choice{.item = std::nullopt, .attack_reason = "attack: KO risk and no medicine left"};
}

std::optional<PolicyState::Choice> PolicyState::row_surf_empty(const Supplies& supplies, const PolicyView&, const Danger&) const{
    if (!surf_empty(supplies.lead)){
        return std::nullopt;
    }
    for (const std::string_view item : {std::string_view("elixir"), std::string_view("max-elixir")}){
        if (bag_count(supplies, item) > 0){
            return Choice{.item = medicine_item(std::string(item), std::format("{}: Surf has no PP left", item))};
        }
    }
    return std::nullopt;
}

std::optional<PolicyState::Choice> PolicyState::row_setup(const Supplies& supplies, const PolicyView& view, const Danger& danger) const{
    const std::optional<PolicyItem> planned = budgeted_setup(supplies, view);
    if (!planned){
        return Choice{.item = std::nullopt, .attack_reason = "attack: no setup left in the goals or the budget, or the hit KOs"};
    }
    const int turns = setup_turns(supplies, view, planned->item == "guard-spec");
    const double reserve = std::min(view.max_hp * 0.75, danger.normal_max + std::max(0, turns - 1) * danger.mean);
    if (view.hp >= reserve){
        return Choice{.item = planned};
    }
    if (std::optional<PolicyItem> item = reserve_heal(supplies, view, danger, reserve)){
        return Choice{.item = std::move(item)};
    }
    return Choice{
        .item = std::nullopt,
        .attack_reason = std::format("attack: HP {:.0f} under the setup reserve {:.0f} and no heal in the budget", view.hp, reserve),
    };
}

std::string PolicyState::state_line(const Supplies& supplies, const PolicyView& view) const{
    return std::format(
        "HP {:.0f}/{:.0f}, foe {} {:.0f}%, Sp. Atk {:+}, Speed {:+}, accuracy {:+}, budget {}{}{}",
        view.hp, view.max_hp, supplies.foe_out.empty() ? "?" : supplies.foe_out, 100 * view.foe_fill,
        m_spa, m_speed, m_accuracy, supplies.budget ? budget_text(*supplies.budget) : std::string("none"),
        m_attack_reason.empty() ? "" : "; ", m_attack_reason
    );
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

void PolicyState::note_battle_text(std::string_view letters){
    const auto has = [letters](std::string_view word){
        return letters.find(word) != std::string_view::npos;
    };
    //  pokefirered's battle strings: "{foe} took\nin sunlight!", "{foe} hid\nunderwater!",
    //  "The sunlight got bright!", "The sunlight faded."
    if ((has("took") && has("sunlight")) || has("underwater")){
        m_foe_charging = true;
        return;
    }
    if (has("sunlight") && has("bright")){
        m_sun_turns = SUN_TURNS;
        return;
    }
    if (has("sunlight") && has("faded")){
        m_sun_turns = 0;
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
