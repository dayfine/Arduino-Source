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
#include "PokemonFRLG_AutoStoryBag.h"
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

//  Whether the battle's text box reads the game's answer to RUN in a
//  trainer's battle, "No! There's no running from a trainer battle!", by
//  its first line. English only: in another language it reads false, and
//  BattleDecisions stops running after a few tries anyway.
bool run_refused_shows(Language language, const ImageViewRGB32& screen);

//  Whether the overworld's message box reads "REPEL's effect wore off...",
//  by its first line, the text still being typed or not (console B,
//  2026-11-13, Route 21). English only: false in another language.
bool repel_wore_off_shows(Language language, const ImageViewRGB32& screen);

//  The bag's answer to USE on a Repel on `screen`, by its message box's
//  two lines. English only: UNREAD in another language. The box is the
//  overworld's; not checked on a bag frame yet.
RepelUse read_repel_use(Language language, const ImageViewRGB32& screen);



//  (name, JSON text) of each file in BattleData/, sorted by name.
//  Generated at build time (SerialPrograms/cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& autostory_battle_json();


//  Registers AutoStory battle data unit tests.
void add_tests_AutoStoryBattleData(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
