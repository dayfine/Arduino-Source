/*  AutoStory Battle Track
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a League fight's screen doesn't show, kept as PokemonSimulation's
 *  engine has it, from the items we used and the battle texts
 *  (read_battle_text(): lower-case letters only): both sides' stat stages,
 *  the foe's status, the sun or rain, the foe's Hyper Beam recharge and
 *  two-turn charge, Guard Spec's Mist and the trainer's Full Restores.
 *  Our stages rise from our X items only; the texts lower them. The texts
 *  move the foe's both ways.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleTrack_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleTrack_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryPlannedState.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  One side's stat stages, -6 to +6.
struct StatStages{
    int attack = 0;
    int defense = 0;
    int sp_attack = 0;
    int sp_defense = 0;
    int speed = 0;
    int accuracy = 0;
    int evasion = 0;
};

//  The weather that changes damage (Sunny Day, Rain Dance).
enum class FieldWeather{
    NONE,
    SUN,
    RAIN,
};

//  The fight's state as tracked.
struct BattleTrackState{
    StatStages own = {};
    int mist_turns = 0;
    StatStages foe = {};
    PlannedStatus foe_status = PlannedStatus::NONE;
    //  The fewest turns the foe's sleep has left: 3 after Rest, else 2,
    //  one less for each turn it sleeps through ("is fast asleep").
    int foe_sleep_turns = 0;
    bool foe_recharging = false;
    //  The two-turn move the foe is charging.
    std::optional<std::string> foe_charging = std::nullopt;
    FieldWeather weather = FieldWeather::NONE;
    //  The choices the weather still lasts for.
    int weather_turns = 0;
    int trainer_full_restores = 0;
};

//  A League fight's tracked state, turn by turn.
class BattleTrack{
public:
    //  The battle texts since the last choice, in order.
    void read_turn(const std::vector<std::string>& texts);
    //  An X item or Guard Spec we used.
    void item_used(std::string_view item);
    //  The foe out is the one at its trainer's `slot`: a new one starts at
    //  stage 0 with no status, and its Intimidate cuts our Attack unless
    //  Mist is up.
    void foe_out(size_t slot, std::string_view ability);
    //  Our lead back in after Roar or Whirlwind: its stages are gone, the
    //  side's Mist stays.
    void lead_switched_in();
    //  A choice made: Mist and the weather count down.
    void turn_chosen();
    const BattleTrackState& state() const{
        return m_state;
    }

private:
    BattleTrackState m_state;
    std::optional<size_t> m_foe_slot;
    //  The foe whose Intimidate our lead has met.
    std::optional<size_t> m_intimidated_by;
};


//  Registers the battle track tests.
void add_tests_AutoStoryBattleTrack(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
