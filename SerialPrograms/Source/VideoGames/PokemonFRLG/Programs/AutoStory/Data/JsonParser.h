/*  FRLG Checkpoint JSON Parser
 *  From: https://github.com/PokemonAutomation/
 */

#ifndef PokemonAutomation_PokemonFRLG_CheckpointJsonParser_H
#define PokemonAutomation_PokemonFRLG_CheckpointJsonParser_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Read a data-authored checkpoint, retaining native bookkeeping defaults.
//  Unknown envelope fields/menu values and malformed common actions throw ParseException.
StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source);

//  Load an embedded authored plan by filename without .json; no controller IO.
StoryCheckpoint load_authored_checkpoint(std::string_view file);


//  Embedded StoryData/*.json records: (filename without .json, text), sorted by name.
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();


//  Migration only: read a captured native-table export, including all bookkeeping.
//  Validate the format/game/status/state basis/unbound list and require a nonempty
//  source revision. Unknown fields/values and missing native fields throw ParseException.
//  The reader cannot verify the source revision against the captured table.
StoryCheckpoint read_story_checkpoint_export(const JsonValue& value, std::string_view source);

//  Migration only: read an embedded export capture by filename without .json.
//  Throws InternalProgramError if the file is not built into the program.
StoryCheckpoint load_story_checkpoint(std::string_view file);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

#endif
