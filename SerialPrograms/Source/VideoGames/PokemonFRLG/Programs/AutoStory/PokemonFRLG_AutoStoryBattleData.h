/*  AutoStory Battle Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game's data the story's attacks are chosen from, generated from
 *  pret/pokefirered by devtools/maps/frlg_battle_data.py into BattleData/
 *  and built into the program: the type chart, every species' types and
 *  base stats, every move's power, type, PP, accuracy, priority and
 *  effect, and every trainer's AI flags, items and party. Also which
 *  trainer each of the story's battle steps fights, and how carefully
 *  (BattleRisk).
 *
 *  Names are the program's slugs ("water-gun", "mr-mime"); trainers are
 *  pret's constants ("TRAINER_SUPER_NERD_MIGUEL").
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleData_H

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
#include "PokemonFRLG_AutoStoryStoryTrainers.h"

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



//  (name, JSON text) of each file in BattleData/, sorted by name.
//  Generated at build time (SerialPrograms/cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& autostory_battle_json();


//  Registers AutoStory battle data unit tests.
void add_tests_AutoStoryBattleData(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
