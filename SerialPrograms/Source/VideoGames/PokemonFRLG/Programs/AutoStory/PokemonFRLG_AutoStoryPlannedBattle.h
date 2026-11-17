/*  AutoStory Planned Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A League fight fought by the planned policy
 *  (PokemonFRLG_AutoStoryPlannedPolicy.h): each turn's PlannedState from
 *  the lead as the story knows it, the foe out as the League foes data has
 *  it (PokemonFRLG_AutoStoryLeagueFoes.h) and what the items used and the
 *  battle texts tell (PokemonFRLG_AutoStoryBattleTrack.h), with every
 *  move's damage from Gen3Damage as @smogon/calc gives it: both sides'
 *  stages, the sun or rain, a burn, Guts and Thick Fat.
 *
 *  Not modelled: the foe's PP (full, where the sim's engine counts it
 *  down), Reflect, and a sleep's exact length (the fewest turns it can
 *  last, where the sim knows it).
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
#include "PokemonFRLG_AutoStoryBattleTrack.h"
#include "PokemonFRLG_AutoStoryLeagueFoes.h"
#include "PokemonFRLG_AutoStoryPartyMember.h"
#include "PokemonFRLG_AutoStoryPlannedPolicy.h"
#include "PokemonFRLG_AutoStoryPlannedState.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The foe out and its trainer.
struct PlannedFoeTrack{
    LeagueFoeData foe = {};
    size_t slot = 0;
    //  Its HP bar's filled share.
    double fill = 1.0;
    int party_alive = 1;
    int full_restores_carried = 0;
};

//  What planned_state() reads.
struct PlannedInputs{
    PartyMember lead = {};
    int hp = 0;
    StatusCondition status = StatusCondition::NONE;
    BattleTrackState track = {};
    PlannedFoeTrack foe = {};
    SlugCounts bag = {};
};

//  The policy's state: our moves' damage on the foe and its moves' on us
//  as the track has the fight, with the badges' boosts on our stats (all
//  eight held at the League). `moves` is the League foes data's move table.
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
    PlannedFoeTrack foe_out(const Supplies& supplies, const PolicyView& view);

    //  Built-in data, never null.
    const LeagueFightData* m_fight;
    const LeagueTrainerData* m_trainer;
    BattleTrack m_track;
    PlannedStreak m_streak;
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
