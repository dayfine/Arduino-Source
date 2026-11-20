/*  PokemonLZA AutoStory - Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The built-in story plan data, checkpoint metadata, lead-in validation,
 *  and StoryData JSON loading.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryPlan_H
#define PokemonAutomation_PokemonLZA_AutoStoryPlan_H

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryStarts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

struct PlzaConsoleHooks;


//  The checkpoints the plans' "execution" blocks mark, by id.
struct MarkedCheckpoints{
    std::set<std::string, std::less<>> autosaves = {};
    std::set<std::string, std::less<>> unverified_autosaves = {};
    std::set<std::string, std::less<>> reload_when_chained = {};
};

//  The story's checkpoints in order, and the checkpoints their plans mark.
struct StoryData : MarkedCheckpoints{
    std::vector<Pokemon::StoryCheckpoint> checkpoints = {};
};

//  The action of the checkpoint's first step; empty for a checkpoint with no steps.
std::string_view checkpoint_first_action(const Pokemon::StoryCheckpoint& checkpoint);

//  True when the checkpoint starts a new game without a prior save.
bool checkpoint_starts_without_save(const Pokemon::StoryCheckpoint& checkpoint);

//  True when the checkpoint makes the new-game intro choices.
bool checkpoint_makes_new_game_choices(const Pokemon::StoryCheckpoint& checkpoint);

//  "<step>: <rule>" for each step of `checkpoint` whose predecessor violates lead-in rules.
std::vector<std::string> checkpoint_lead_in_errors(const JsonObject& checkpoint);

//  (name, text) by name, embedded by cmake/EmbedTextFiles.cmake.
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  Parses plan file `text` with `actions` and records its execution marks in `marked`.
//  Throws ParseException naming `source` on parse, lead-in, or autosave-step error.
Pokemon::StoryCheckpoint read_plza_checkpoint(
    std::string_view text, std::string_view source, Pokemon::StoryActions& actions, MarkedCheckpoints& marked,
    std::optional<std::string_view> previous
);

//  Loads the built-in story bound to `hooks`, which must outlive the returned StoryData.
//  Throws ParseException naming any file that fails to parse.
StoryData load_plza_story(PlzaConsoleHooks& hooks);

//  Built-in StoryData loaded once with no hooks (for metadata and inspection only).
const StoryData& builtin_story_data();

//  Built-in plans' start signatures in story order.
const std::vector<CheckpointStart>& builtin_checkpoint_starts();

//  True if any start signature up to `asked` requires reading the full map.
bool builtin_start_reads_map(std::string_view asked);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
