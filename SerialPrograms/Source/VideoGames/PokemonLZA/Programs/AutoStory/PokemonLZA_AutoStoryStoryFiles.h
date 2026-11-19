/*  PokemonLZA AutoStory - Story files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The built-in story, read from StoryData/plza-<id>.plan.json.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStoryFiles_H
#define PokemonAutomation_PokemonLZA_AutoStoryStoryFiles_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Data/Pokemon_StoryFiles.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  (name, text) by name, embedded by cmake/EmbedTextFiles.cmake.
//  code-health: allow test_only_api -- the generated embed file (cmake/EmbedTextFiles.cmake) defines it from this declaration
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  Read once. Throws ParseException naming the file that does not parse.
const StoryData& builtin_story_data();

//  The plans' "start_signature"s, read once, in story order; two may share one.
const std::vector<CheckpointStart>& builtin_checkpoint_starts();

//  Whether a start up to `asked` names wild_zone or a minimap, so the run
//  opens the full map for the pin (the map opens only after 03.02b).
bool builtin_start_reads_map(std::string_view asked);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
