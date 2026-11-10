/*  AutoStory Battle Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game's data the story's attacks are chosen from when the plan has
 *  none, built into the program from BattleData/: the type chart, each
 *  species' types, and each move's type, whether it does damage and its
 *  PP. A battle's foes come from its step in the checkpoint's plan.
 *
 *  Names are the program's slugs ("water-gun", "mr-mime").
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleData_H

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"
#include "PokemonFRLG_AutoStoryBattleRisk.h"
#include "PokemonFRLG_AutoStoryMoveData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  Every species, for reading a wild Pokemon's name.
const std::set<std::string>& battle_species();

//  The foes of a wild battle whose Pokemon was read as `species`: it, or
//  none when it wasn't read ("") or the data doesn't have it.
std::vector<std::string> wild_foes(std::string_view species);

//  The foes of the wild battle on `screen`, a battle menu: the Pokemon its
//  HP box names (WildEncounterReader, among battle_species()), as
//  wild_foes() takes it. Logs what it read.
std::vector<std::string> read_wild_foes(Logger& logger, Language language, const ImageViewRGB32& screen);

//  The Pokemon a trainer's battle shows on `screen`, a battle menu: the
//  name its HP box shows (WildEncounterReader), among `party`'s species.
//  Empty if not read. Logs what it read.
std::string read_trainer_foe(Logger& logger, Language language, const ImageViewRGB32& screen, const std::vector<std::string>& party);



//  (name, JSON text) of each file in BattleData/, sorted by name.
//  Generated at build time (SerialPrograms/cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& autostory_battle_json();


//  Registers AutoStory battle data unit tests.
void add_tests_AutoStoryBattleData(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
