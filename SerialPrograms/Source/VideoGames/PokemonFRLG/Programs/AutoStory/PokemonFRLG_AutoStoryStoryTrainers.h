/*  AutoStory Story Trainers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which trainer each of the story's battle steps fights, and how
 *  carefully (BattleRisk).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStoryTrainers_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStoryTrainers_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"
#include "Pokemon/Pokemon_Types.h"
#include "PokemonFRLG_AutoStoryTrainerData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The trainer the story's battle step `battle` fights on `map` (the map
//  it leaves the player on). None for a battle the table doesn't name.
std::optional<std::string_view> story_trainer(std::string_view map, std::string_view battle);

//  How carefully a battle step is fought.
enum class BattleRisk{
    //  The move with the best expected damage against the foe out.
    LOW,
    //  The battles the simulation's plan is for (the rival, gym leaders,
    //  bosses); fought as LOW until the checkpoint carries a plan.
    HIGH,
};
//  String representation of battle risk level.
std::string_view battle_risk_name(BattleRisk risk);

//  A trainer's risk by its kind: HIGH for the rival, gym leaders, bosses
//  (Giovanni, the Rocket admins), the Elite Four and the Champion; LOW
//  for the rest.
BattleRisk default_battle_risk(std::string_view trainer);

//  The risk of the story's battle step `battle` on `map`: its table row's,
//  else its trainer's default. LOW for a battle the table doesn't name.
BattleRisk story_battle_risk(std::string_view map, std::string_view battle);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
