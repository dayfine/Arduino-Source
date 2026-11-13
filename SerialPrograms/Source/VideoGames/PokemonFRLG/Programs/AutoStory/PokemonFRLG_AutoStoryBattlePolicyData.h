/*  AutoStory Battle Policy Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a trainer battle's plan step says of how it is fought (see
 *  PokemonFRLG_AutoStoryBattlePolicy.h): the policy, the setup goals and
 *  what each foe can do to the lead.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattlePolicyData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattlePolicyData_H

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How a battle's items are chosen: not by the policy, PokeGuy's B (cure,
//  heal) or C2 (B, then setup towards goals).
enum class BattlePolicyKind{
    NONE,
    SOLO_RECOVERY,
    GOAL_BASED_SETUP,
};

//  The doc's setup goals for one fight.
struct SetupGoals{
    //  Sp. Atk stage to reach.
    int spa = 0;
    //  Accuracy stage to reach, plus the foe's evasion stages.
    int accuracy = 0;
    //  X Speed until strictly faster than every foe left.
    bool outspeed = false;
    //  Guard Spec first when the foe out has an accuracy-lowering move.
    bool guard_accuracy = false;
    //  Raise the Sp. Atk goal while Torrent is off, to make up for it.
    bool torrent_power = false;
    //  The same, in the opening only (the first foe, with more to come):
    //  the doc's Torrent stall, whose extra X Specials stand in for the
    //  turns that wait for Torrent. Not repeated on later foes.
    bool target_torrent = false;
};

//  What one foe can do to the lead, from the doc's stats: the HP the lead
//  loses to it in a turn (the policy's sampled 80th percentile, mean and
//  maximum, and the largest ordinary hit), and its Speed.
struct FoeThreat{
    int speed = 0;
    int likely = 0;
    int mean = 0;
    int max = 0;
    int normal_max = 0;
    //  Sand Attack, Smokescreen, Kinesis and the like.
    bool lowers_accuracy = false;
    //  Self-Destruct, Explosion.
    bool explodes = false;
};

//  A step's policy: the kind, its goals and the foes' threat rows.
struct BattlePolicy{
    BattlePolicyKind kind = BattlePolicyKind::NONE;
    SetupGoals goals = {};
    //  In the step's "foes" order.
    std::vector<FoeThreat> foes = {};
};

//  The policy a step's "policy" word names; none for an empty or unknown one.
std::optional<BattlePolicyKind> battle_policy_kind(std::string_view word);

//  The battle policy a step's args name, with its setup goals and foe data:
//  none when they name none.
BattlePolicy battle_policy_of(const Pokemon::StoryActionArgs& args);

//  The step's "foe_threats" rows, [speed, likely, mean, max, normal_max,
//  lowers_accuracy, explodes] each; none if a row isn't that.
std::optional<std::vector<FoeThreat>> foe_threats_of(const std::vector<std::vector<int64_t>>& rows);




} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
