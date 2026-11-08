/*  FRLG Authored Checkpoint Export
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_PokemonFRLG_AutoStoryExport_H
#define PokemonAutomation_PokemonFRLG_AutoStoryExport_H
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonFRLG_AutoStoryEvents.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
//  Capture evaluated shared fields and every FRLG checkpoint extension.
//  Native supplies are bookkeeping, not observed state or entry requirements.
//  Runtime options, map pins, effective policies and execution remain unbound.
JsonValue story_checkpoint_export(const StoryCheckpoint& checkpoint);
//  The inverse: a row from a `pokesim-native-checkpoint-export/1` record for
//  FRLG, as story_checkpoints() would hold it, for build_checkpoint(). The
//  envelope's format, game, status, state basis and unbound list must be the
//  ones the export writes, and `source_revision` must name a revision; the
//  loader cannot check which. Every native field is required: a value the
//  record lacks is not a default. Unknown fields and values fail (ParseException).
StoryCheckpoint read_story_checkpoint_export(const JsonValue& value, std::string_view source);
//  The checkpoint records built into the program from
//  Programs/AutoStory/StoryData/*.json (cmake/EmbedTextFiles.cmake):
//  (file name without ".json", its text), sorted by name.
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();
//  The record `file` of autostory_checkpoint_json(), read with
//  read_story_checkpoint_export(). Throws InternalProgramError for a file
//  that isn't built in.
StoryCheckpoint load_story_checkpoint(std::string_view file);
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
