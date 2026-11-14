/*  PokemonLZA AutoStory - Story files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the story (PokemonLZA_AutoStoryTable.h) is read from: the
 *  checked-in checkpoint files, StoryData/plza-<id>.plan.json, built into
 *  the program, and for lab tuning the files of story_dev_folder("plza")
 *  (Pokemon_StoryDevFiles.h), each in place of the built-in one of the
 *  same name, so a walk can be retimed without a rebuild. The shared
 *  loader (Pokemon_StoryFiles.h) reads them; this is PLZA's reader for it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStoryFiles_H
#define PokemonAutomation_PokemonLZA_AutoStoryStoryFiles_H

#include <functional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Data/Pokemon_StoryDevFiles.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryFiles.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The checked-in checkpoint files, StoryData/plza-<id>.plan.json, built
//  into the program (cmake/EmbedTextFiles.cmake): (name, text), by name.
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  How the shared loader reads a PLZA checkpoint file: its execution has
//  only the shared "save" (required) and "reload_when_chained"; "dev_only"
//  is refused, since PLZA lists every row. A dev file must pass
//  plza_story_lint(), and 01.00 (story_row_starts_without_save()) has no
//  save to reload.
const Pokemon::StoryFileReader<Pokemon::StoryCheckpoint>& plza_story_file_reader();

//  The built-in checkpoints, each of `dev_files` in place of the one of the
//  same file name (Pokemon::load_story_data() with plza_story_file_reader()).
//  Throws ParseException naming the file when a dev file does not parse,
//  fails plza_story_lint(), matches no built-in file or changes its
//  checkpoint's id.
StoryData load_story_data(std::span<const Pokemon::StoryDevFile> dev_files);

//  The built-in story, load_story_data() with no dev files, read once.
const StoryData& builtin_story_data();

//  The built-in story (load_story_data() with no dev files), read once:
//  what the tests, the lint and the program's dropdowns use. 01.xx Main
//  Mission 01 (new game, Pancham, the starter), 02.xx Main Mission 02 (out
//  of the Battle Zone to Hotel Z, the first night), 03.xx Main Mission 03
//  (Taunie's battle, the Pokemon Center, Wild Zone 1, Mable's Rock Smash
//  TM). Only the first, 01.00, starts without a save: it plays a new game
//  to the station platform from the new game's "Confirm Choices", its
//  choices made by hand.
const std::vector<Pokemon::StoryCheckpoint>& story_checkpoints();

//  The built-in rows that end on one of the game's autosaves. Each ends
//  where the game autosaved in the recording, so the next row reloads
//  there. The menu (X, then R) can save from the start of the story too:
//  01.03 saves through it after Andi (console B, 2026-11-10).
const std::set<std::string, std::less<>>& story_autosave_rows();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
