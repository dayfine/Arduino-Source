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
#include <optional>
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryActionTable.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  (name, text) by name, embedded by cmake/EmbedTextFiles.cmake.
//  code-health: allow test_only_api -- the generated embed file (cmake/EmbedTextFiles.cmake) defines it from this declaration
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  The plan file `text`, its steps parsed by `actions`; its execution block
//  adds it to `marked` (`previous`: the checkpoint before it). Throws ParseException
//  naming `source` when it does not parse, fails a step's lead-in, or
//  autosaves short of its end.
Pokemon::StoryCheckpoint read_plza_checkpoint(
    std::string_view text, std::string_view source, Pokemon::StoryActions& actions, MarkedCheckpoints& marked,
    std::optional<std::string_view> previous
);

//  The built-in story, its steps bound to `hooks`, which must outlive it.
//  Throws ParseException naming the file that does not parse.
StoryData load_plza_story(PlzaConsoleHooks& hooks);

//  load_plza_story() with no hooks, read once: for the ids, the execution
//  checkpoints and the steps' names, not to run.
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
