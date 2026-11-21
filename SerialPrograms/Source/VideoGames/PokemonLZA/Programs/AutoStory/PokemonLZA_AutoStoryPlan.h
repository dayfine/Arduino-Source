/*  PokemonLZA AutoStory - Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The built-in story plan data, checkpoint metadata, and StoryData JSON loading.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryPlan_H
#define PokemonAutomation_PokemonLZA_AutoStoryPlan_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryStarts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The story's checkpoints in order, their start signatures, and the checkpoints their plans mark.
struct StoryData : MarkedCheckpoints{
    std::vector<Pokemon::StoryCheckpoint> checkpoints = {};
    std::vector<CheckpointStart> starts = {};
};

//  The action of the checkpoint's first step; empty for a checkpoint with no steps.
std::string_view checkpoint_first_action(const Pokemon::StoryCheckpoint& checkpoint);

//  (name, text) by name, embedded by cmake/EmbedTextFiles.cmake.
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  Loads the built-in story.
//  Throws ParseException naming any file that fails to parse.
StoryData load_plza_story();

//  Built-in StoryData loaded once (for metadata and inspection only).
const StoryData& builtin_story_data();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
