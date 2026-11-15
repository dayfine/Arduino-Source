/*  PokemonLZA AutoStory - Story files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the story (PokemonLZA_AutoStoryTable.h) is read from: the
 *  checked-in checkpoint files, StoryData/plza-<id>.plan.json, built into
 *  the program. The shared loader (Pokemon_StoryFiles.h) reads them; this
 *  is PLZA's reader for it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStoryFiles_H
#define PokemonAutomation_PokemonLZA_AutoStoryStoryFiles_H

#include <string>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Data/Pokemon_StoryFiles.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The checked-in checkpoint files, StoryData/plza-<id>.plan.json, built
//  into the program (cmake/EmbedTextFiles.cmake): (name, text), by name.
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  How the shared loader reads a PLZA checkpoint file: against
//  plza_story_specs(), its execution with only the shared "save" and
//  "reload_when_chained". 01.00 (story_row_starts_without_save()) has no
//  save to reload.
const Pokemon::StoryFileReader<Pokemon::StoryCheckpoint>& plza_story_file_reader();

//  The built-in story, read once. Throws ParseException naming the file
//  when one does not parse. 01.00 plays a new game to the station platform
//  from the new game's "Confirm Choices", its choices made by hand; 01.01
//  walks off the platform into the white fade.
const StoryData& builtin_story_data();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
