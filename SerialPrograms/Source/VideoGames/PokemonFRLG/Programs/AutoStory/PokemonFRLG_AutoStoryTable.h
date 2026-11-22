/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's authored checkpoint plans (StoryData/frlg-*.plan.json), parsed via
 *  #648 Pokemon::read_story_checkpoint() + S10 action parsers and #653
 *  Pokemon_GridStoryArgs, plus FRLG's execution and metadata blocks.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTable_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTable_H

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryDevFiles.h"
#include "PokemonFRLG_AutoStoryCheckpoints.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The loaded FRLG story checkpoints and any dev files applied in their place.
struct FrlgStoryData{
    std::vector<StoryCheckpoint> checkpoints = {};
    //  (filename, path) of each dev file applied.
    std::vector<std::pair<std::string, std::string>> dev_files = {};
};

//  The checkpoints visible in user or developer mode (dev_only hidden in user mode).
std::vector<StoryCheckpoint> frlg_rows_for_mode(const FrlgStoryData& data, bool developer_mode);

//  (name, JSON text) of each file in StoryData/, sorted by name.
//  Generated at build time (SerialPrograms/cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  The checkpoint of an authored FRLG file (parsed via #648
//  Pokemon::read_story_checkpoint()). Throws ParseException when malformed.
StoryCheckpoint read_story_checkpoint_file(const JsonValue& value, std::string_view source);

//  Loads the embedded StoryData/ plans in STORY_ORDER, substituting any
//  matching `dev_files`. Throws ParseException on invalid or unknown files.
FrlgStoryData load_frlg_story(std::span<const Pokemon::StoryDevFile> dev_files);

//  The built-in story's rows in STORY_ORDER.
const std::vector<StoryCheckpoint>& story_checkpoints();


//  Registers AutoStory table validation unit tests.
void add_tests_AutoStory(UnitTestDatabase& database);
//  The checkpoint files read through the loader, with dev files.
void add_tests_AutoStoryStoryFiles(UnitTestDatabase& database);
//  Which rows a run takes.
void add_tests_AutoStoryRunPlan(UnitTestDatabase& database);
//  The table run end to end on StoryEngine against StoryMapsSim.
void add_tests_AutoStorySimulation(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
