/*  BDSP Authored Checkpoint Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryData_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryData_H

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryDevFiles.h"
#include "PokemonBDSP_AutoStory_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The loaded BDSP story checkpoints and any dev files used in their place.
struct BdspStoryData{
    std::vector<StoryCheckpoint> checkpoints = {};
    //  (filename, path) of each dev file applied.
    std::vector<std::pair<std::string, std::string>> dev_files = {};
};

//  The checkpoints visible in user or developer mode (dev_only rows hidden in user mode).
std::vector<StoryCheckpoint> bdsp_rows_for_mode(const BdspStoryData& data, bool developer_mode);

//  The embedded authored JSON files, keyed by filename without .json.
const std::vector<std::pair<std::string, std::string>>& bdsp_checkpoint_json();

//  Strict authored envelope (parsed via Pokemon::read_story_checkpoint()) and
//  BDSP save/failure configuration; no controller IO.
//  Unknown fields/values or inconsistent save policy throw ParseException.
StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source);

//  Load an embedded authored checkpoint; a missing file throws InternalProgramError.
StoryCheckpoint load_authored_checkpoint(std::string_view file);

//  The embedded authored checkpoints in id order, each of `dev_files` in
//  place of the embedded file of its name. Throws ParseException for a bad
//  file, an unknown dev file name, a changed checkpoint id, or duplicate ids.
BdspStoryData load_bdsp_story(std::span<const Pokemon::StoryDevFile> dev_files);

//  The embedded authored checkpoint file, parsed but not read.
JsonValue authored_checkpoint_document(std::string_view file);

//  Parses one BDSP step's "args" object for `action` into a typed BdspStep
//  (leaving name/why for the caller). Throws ParseException on invalid args.
BdspStep parse_bdsp_step(std::string_view action, const JsonObject& args);

//  Builds a Pokemon::StoryActions parser table over all BDSP actions that
//  binds every parsed step into `make_run`.
Pokemon::StoryActions make_bdsp_action_parsers(
    std::function<Pokemon::StoryStepRun(BdspStep)> make_run
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
