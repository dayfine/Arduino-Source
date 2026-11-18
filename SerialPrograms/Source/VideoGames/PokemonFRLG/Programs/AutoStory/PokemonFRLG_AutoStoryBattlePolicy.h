/*  AutoStory Battle Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PokeGuy's battle policy as PokemonSimulation's
 *  docs/battles/pokeguy-conditions.md specifies it, for a trainer battle
 *  whose plan step names one ("policy"): "solo-recovery" (B: cure a
 *  status, heal when the HP is at risk) and "goal-based-setup" (C2: B's
 *  recovery, then X items towards the step's stage goals before attacking;
 *  the steps' setup_* arguments are the doc's setup goals). Pure decisions
 *  from the bag, the lead's HP, speed and stages, and the foe data the
 *  step carries; nothing here reads a screen. The lead's attack is
 *  chosen elsewhere (BattleDecisions::move_turn).
 *
 *  The Torrent stall (target_torrent) is its extra X Specials only: the
 *  doc's other stall, the weakest damaging move that can't KO the setup
 *  foe, is not modelled. Setup pays only while its stages last and the
 *  foe out needs more than one attack: none against a foe that phazes
 *  (Roar, Whirlwind) or that the lead's best attack KOs from full HP at
 *  its lowest roll (the step's foe_stats with HP). Not modelled: the
 *  foe's HP as the fight goes, Oran Berries and PP medicine in battle. Stage drops and evasion rises of the foe are
 *  not read off the battle text yet: foe_evasion_raised() and the like
 *  take them once something does.
 *
 *  "budgeted-setup" (budgeted C2, the League's; design and sim:
 *  ~/BDSPLab/notes/frlg-e4-replica-policy.md). One state -> action table
 *  (choose_budgeted()'s rows), first row that applies, from the screen
 *  state each turn; D is the foe's best ordinary hit (foe_threats'
 *  normal_max, plus poison; for a foe whose hit charges a turn first, only
 *  while its charge text or Sunny Day's sun was read: PolicyView::battle_texts),
 *  "medicine" an item on the lead:
 *    1. awake, we move first and our lowest roll KOs the foe at its HP
 *       bar: attack, at any HP (PokeGuy's Surf at 3/171), with Strength
 *       when its lowest roll KOs (Bite and Surf keep their PP);
 *    2. Strength's lowest roll KOs and the foe can't KO first (HP > D):
 *       Strength (Bruno, console B, 2026-11-16: Surf on Hitmonchan at 4%
 *       and 2%);
 *    3. the last two turns were medicine and the foe's bar didn't move:
 *       at KO risk a Full Restore (or Max Potion), unless the last was
 *       one; else attack. Never a third heal in a row (Lorelei's Lapras,
 *       console B, 2026-11-16: six turns of Super Potions and Parlyz
 *       Heals);
 *    4. a status and HP <= D: a Full Restore, over the budget if need be;
 *    5. asleep: an Awakening, else a Full Heal; with none, attack
 *       (Agatha's Gengar, 2026-11-16: two turns asleep, then Dream Eater);
 *    6. a status with a cure and HP > D: the cure, else a Full Heal;
 *    7. HP <= D (KO risk): the smallest medicine that lifts HP above D and
 *       heals at least D plus the foe's mean hit (a Hyper or Max Potion
 *       only within the budget), else a Full Restore, over the fight's
 *       budget if need be; none helps (D >= max HP): attack;
 *    8. Surf out of PP: an Elixir, else a Max Elixir;
 *    9. setup (Guard Spec, X Special, X Speed, X Accuracy towards the
 *       step's goals; accuracy goal without the foe's evasion), each item
 *       capped by the fight's budget (Supplies::budget), none when the
 *       next hit KOs the foe or the foe phazes; HP under C2's reserve
 *       min(75% max HP, D + (setup turns - 1) x mean hit) first heals
 *       within the budget, else the setup is dropped;
 *   10. attack.
 *  The budget is the battle's Supplies::budget (none: no cap);
 *  every turn's state and choice is logged on one line (state_line()).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattlePolicy_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattlePolicy_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_AutoStoryBattlePolicyData.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What the policy reads of the lead.
struct PolicyView{
    //  Its HP, from the bar, and its full HP.
    double hp = 0;
    double max_hp = 0;
    StatusCondition status = StatusCondition::NONE;
    //  Its Speed as POKEMON SKILLS shows it, before badges and stages.
    int speed = 0;
    //  The foe out, as its slot in the step's "foes" (FoeParty: the
    //  trainer may send them in any order).
    size_t foe_index = 0;
    //  The foe's HP bar's filled share, 1 when not read.
    double foe_fill = 1.0;
    //  The battle texts read since the last choice, letters in lower case
    //  (read_battle_text()): the foe charging a hit (Solar Beam, Dive)
    //  makes its row a KO risk for this choice; Sunny Day's sun makes
    //  Solar Beam one for its turns.
    std::vector<std::string> battle_texts = {};
};

//  An item the policy uses this turn.
struct PolicyItem{
    std::string item;
    //  On the lead through the party list (a medicine); else on the Pokemon
    //  out (an X item).
    bool party_member = false;
    //  For the log.
    std::string why;
};

//  How many of `item` the bag holds, as far as known; 0 if unknown.
int bag_count(const Supplies& supplies, std::string_view item);

//  The HP the HP medicine `item` heals (infinity for a whole heal); 0 for any other item.
double medicine_hp(std::string_view item);

//  The lead's stages the policy tracks, and the turns of Mist left.
struct PolicyStages{
    int spa = 0;
    int speed = 0;
    int accuracy = 0;
    int mist_turns = 0;
};

//  A trainer battle's plan is over: its party, items and policy go from
//  `supplies`.
void end_trainer_battle(Supplies& supplies);

//  The lead's side of the battle as the policy tracks it: the stages and
//  Mist from the items used and the effects reported.
class PolicyState{
public:
    explicit PolicyState(const BattlePolicy& policy);

    //  A turn's choice: the item to use, or none to attack, with the move
    //  the policy names for it ("strength"; none leaves it to the move choice).
    struct Turn{
        std::optional<PolicyItem> item = std::nullopt;
        std::optional<std::string> move = std::nullopt;
    };

    //  The choice at a battle menu.
    Turn choose(const Supplies& supplies, const PolicyView& view);
    //  `item` was used: its stage, or Mist, applies.
    void item_used(const std::string& item);

    void foe_evasion_raised(int stages);
    void own_accuracy_lowered(int stages);
    void own_speed_lowered(int stages);

    PolicyStages stages() const{
        return {.spa = m_spa, .speed = m_speed, .accuracy = m_accuracy, .mist_turns = m_mist_turns};
    }
    //  The turn's state for the log: our HP, the foe and its bar, our
    //  stages and the budget left; after a choose() that attacks, why.
    std::string state_line(const Supplies& supplies, const PolicyView& view) const;

private:
    struct Danger{
        double likely = 0;
        double mean = 0;
        double max = 0;
        double normal_max = 0;
    };
    Danger danger(const PolicyView& view) const;
    const FoeThreat* foe(const PolicyView& view) const;
    void note_battle_text(std::string_view letters);
    int own_speed(const PolicyView& view) const;
    bool outsped(const PolicyView& view) const;
    int spa_goal(const PolicyView& view) const;
    int accuracy_goal() const;
    std::optional<PolicyItem> cure(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<PolicyItem> heal(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<PolicyItem> setup(const Supplies& supplies, const PolicyView& view) const;
    //  `waiting_on` is for the log: the setup the HP reserve is for, or that none is pending.
    std::optional<PolicyItem> planning_heal(
        const Supplies& supplies, const PolicyView& view, const Danger& danger, int turns, std::string_view waiting_on
    ) const;
    int setup_turns(const Supplies& supplies, const PolicyView& view, bool guard) const;
    //  BUDGETED_SETUP's table (the header): each row is the turn's choice
    //  when it applies, none when it doesn't.
    struct Choice{
        std::optional<PolicyItem> item;
        //  An attack's reason, for the log, and its move if the row names one.
        std::string attack_reason = {};
        std::optional<std::string> move = std::nullopt;
    };
    Turn choose_budgeted(const Supplies& supplies, const PolicyView& view);
    std::optional<PolicyItem> choose_unbudgeted(const Supplies& supplies, const PolicyView& view);
    std::optional<Choice> row_sure_ko(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<Choice> row_strength_finisher(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<Choice> row_escalate(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<Choice> row_status_at_ko_risk(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<Choice> row_asleep(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<Choice> row_status_cure(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<Choice> row_ko_risk(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<Choice> row_surf_empty(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<Choice> row_setup(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    bool sure_ko_first(const Supplies& supplies, const PolicyView& view) const;
    bool strength_kos(const Supplies& supplies, const PolicyView& view) const;
    std::optional<PolicyItem> budgeted_setup(const Supplies& supplies, const PolicyView& view) const;
    std::optional<PolicyItem> ko_risk_heal(const Supplies& supplies, const PolicyView& view, const Danger& danger) const;
    std::optional<PolicyItem> reserve_heal(
        const Supplies& supplies, const PolicyView& view, const Danger& danger, double reserve
    ) const;

    const BattlePolicy m_policy;
    int m_spa = 0;
    int m_speed = 0;
    int m_accuracy = 0;
    int m_foe_evasion = 0;
    int m_mist_turns = 0;
    //  The foe's charge text was read since the last choice; Sunny Day's turns left.
    bool m_foe_charging = false;
    int m_sun_turns = 0;
    std::string m_attack_reason;
    //  The medicine turns in a row against the foe's bar m_streak_fill
    //  (foe m_streak_foe), and the last one's item.
    int m_medicine_streak = 0;
    size_t m_streak_foe = 0;
    double m_streak_fill = 1.0;
    std::string m_last_medicine;
};


//  Registers the battle policy unit tests.
void add_tests_AutoStoryBattlePolicy(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
