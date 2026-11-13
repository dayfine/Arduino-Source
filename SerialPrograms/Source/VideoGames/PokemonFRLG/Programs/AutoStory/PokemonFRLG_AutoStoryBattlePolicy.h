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
 *  foe, needs the foe's HP. Not modelled: the foe's HP (so the doc's safe
 *  finishing hit is never taken early), Oran Berries and PP medicine in
 *  battle. Stage drops and evasion rises of the foe are
 *  not read off the battle text yet: foe_evasion_raised() and the like
 *  take them once something does.
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
    //  The foe out, in the step's "foes" order; those before it have fainted.
    size_t foe_index = 0;
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

//  A trainer battle's plan is over: its party, items and policy go from `supplies`.
void end_trainer_battle(Supplies& supplies);

//  The lead's side of the battle as the policy tracks it: the stages and
//  Mist from the items used and the effects reported.
class PolicyState{
public:
    explicit PolicyState(const BattlePolicy& policy);

    //  The item to use at a battle menu, or none to attack.
    std::optional<PolicyItem> choose(const Supplies& supplies, const PolicyView& view);
    //  `item` was used: its stage, or Mist, applies.
    void item_used(const std::string& item);

    void foe_evasion_raised(int stages);
    void own_accuracy_lowered(int stages);
    void own_speed_lowered(int stages);

    PolicyStages stages() const{
        return {.spa = m_spa, .speed = m_speed, .accuracy = m_accuracy, .mist_turns = m_mist_turns};
    }

private:
    struct Danger{
        double likely = 0;
        double mean = 0;
        double max = 0;
        double normal_max = 0;
    };
    Danger danger(const PolicyView& view) const;
    const FoeThreat* foe(const PolicyView& view) const;
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

    const BattlePolicy m_policy;
    int m_spa = 0;
    int m_speed = 0;
    int m_accuracy = 0;
    int m_foe_evasion = 0;
    int m_mist_turns = 0;
};


//  Registers the battle policy unit tests.
void add_tests_AutoStoryBattlePolicy(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
