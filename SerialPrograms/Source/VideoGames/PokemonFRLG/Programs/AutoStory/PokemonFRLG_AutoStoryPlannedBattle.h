/*  AutoStory Planned Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A League fight fought by the planned policy
 *  (PokemonFRLG_AutoStoryPlannedPolicy.h): each turn's PlannedState from
 *  the lead as the story knows it, the foe out as the League foes data has
 *  it (PokemonFRLG_AutoStoryLeagueFoes.h) and the battle texts, with every
 *  move's damage from Gen3Damage at the stages we track.
 *
 *  Tracked: our Attack (Intimidate), Sp. Atk, Speed and accuracy stages
 *  and Guard Spec's Mist from the items used, all reset when the lead is
 *  switched back in after Roar or Whirlwind; the trainer's Full Restores
 *  ("<trainer> used FULL RESTORE!"); the foe's Hyper Beam recharge, its
 *  Solar Beam or Dive charge and Sunny Day's sun, from the texts.
 *  Not modelled: the weather's damage boost, the foe's stages and status,
 *  our Defense stages.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlannedBattle_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlannedBattle_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryLeagueFoes.h"
#include "PokemonFRLG_AutoStoryPartyMember.h"
#include "PokemonFRLG_AutoStoryPlannedPolicy.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Our stages and Mist's turns left.
struct PlannedStages{
    int attack = 0;
    int spa = 0;
    int speed = 0;
    int accuracy = 0;
    int mist_turns = 0;
};

//  The foe out and its trainer, as the texts tell.
struct PlannedFoeTrack{
    LeagueFoeData foe = {};
    size_t slot = 0;
    //  Its HP bar's filled share.
    double fill = 1.0;
    int party_alive = 1;
    int full_restores_carried = 0;
    int full_restores_used = 0;
    bool recharging = false;
    std::optional<std::string> charging = std::nullopt;
    bool sun = false;
};

//  What planned_state() reads.
struct PlannedInputs{
    PartyMember lead = {};
    int hp = 0;
    StatusCondition status = StatusCondition::NONE;
    PlannedStages stages = {};
    PlannedFoeTrack foe = {};
    SlugCounts bag = {};
};

//  The policy's state: our moves' damage on the foe and its moves' on us
//  at our stages, with the badges' boosts on our stats (all eight held at
//  the League). `moves` is the League foes data's move table.
PlannedState planned_state(const PlannedInputs& inputs, const LeagueFoesData& moves);


//  A League fight's turns.
class PlannedBattle{
public:
    PlannedBattle(const LeagueFightData& fight, const LeagueTrainerData& trainer);

    //  A turn's choice, and its line for the log.
    struct Turn{
        PlannedChoice choice = {};
        std::string log = {};
    };
    Turn choose(const Supplies& supplies, const PolicyView& view);
    void item_used(std::string_view item);
    //  Back in after Roar or Whirlwind: its stages are gone.
    void lead_switched_in();

private:
    void read_texts(const std::vector<std::string>& texts);
    PlannedFoeTrack foe_out(const Supplies& supplies, const PolicyView& view);

    //  Built-in data, never null.
    const LeagueFightData* m_fight;
    const LeagueTrainerData* m_trainer;
    PlannedStages m_stages;
    PlannedStreak m_streak;
    int m_full_restores_used = 0;
    bool m_recharging = false;
    std::optional<std::string> m_charging;
    int m_sun_turns = 0;
    std::optional<size_t> m_last_slot;
};

//  The League fight `trainer` (a battle step's "trainer") is fought by
//  the planned policy: its plan and its foes data, or none for another
//  trainer.
std::optional<PlannedBattle> planned_battle(std::string_view trainer);


//  Registers the planned battle tests.
void add_tests_AutoStoryPlannedBattle(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
