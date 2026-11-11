/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story as rows of the shared story steps
 *  (Pokemon/AutoStory/Types/Pokemon_StoryStep.h), plain data as BDSP's and
 *  FRLG's tables are, so unit tests can check it without a console.
 *
 *  Lumiose has no tile grid, so a row's poses name a place and nothing more,
 *  and a walk in the overworld is a GAME "walk" step whose args hold its
 *  legs: a relative turn and a walk time each (PokemonLZA_AutoStoryGame.h).
 *
 *  Recorded 2026-11-08/09 on console B, played step by step from a new
 *  game to the quest "Return to Hotel Z to meet up with Taunie" in Main
 *  Mission 03. Every walk was measured walking, not dashing. Steps the game
 *  has no code for yet are GAME steps of story_actions_not_implemented().
 *
 *  For lab tuning, a run reads the checkpoint files of story_dev_folder()
 *  in place of the built-in ones of the same name at each start, so a walk
 *  can be retimed without a rebuild.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTable_H
#define PokemonAutomation_PokemonLZA_AutoStoryTable_H

#include <functional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryDevFiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The checked-in checkpoint files, StoryData/plza-<id>.plan.json, built
//  into the program (cmake/EmbedTextFiles.cmake): (name, text), by name.
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  The story a run plays: its checkpoints in order, and the ids of those
//  that end on one of the game's autosaves and do not save themselves
//  (their JSON "execution": {"save": "autosave"}).
struct StoryData{
    std::vector<Pokemon::StoryCheckpoint> checkpoints;
    std::set<std::string, std::less<>> autosaves;
    //  The dev files that replaced a built-in checkpoint, as (file name,
    //  path).
    std::vector<std::pair<std::string, std::string>> dev_files;
};

//  The built-in checkpoints, each of `dev_files` in place of the one of the
//  same file name. Throws ParseException naming the file when a dev file
//  does not parse, fails plza_story_lint(), matches no built-in file or
//  changes its checkpoint's id.
StoryData load_story_data(std::span<const StoryDevFile> dev_files);

//  The built-in story (load_story_data() with no dev files), read once:
//  what the tests, the lint and the program's dropdowns use. 01.xx Main
//  Mission 01 (new game, Pancham, the starter), 02.01 Main Mission 02 (out
//  of the Battle Zone to Hotel Z, the first night), 03.xx Main Mission 03
//  (Taunie's battle, the Pokemon Center, Wild Zone 1, Mable's Rock Smash
//  TM). Only 01.00, a new game, starts without a save; it starts on the
//  title screen.
const std::vector<Pokemon::StoryCheckpoint>& story_checkpoints();

//  The built-in rows that end on one of the game's autosaves. Each ends
//  where the game autosaved in the recording, so the next row reloads
//  there. The menu (X, then R) can save from the start of the story too:
//  01.03 saves through it after Andi (console B, 2026-11-10).
const std::set<std::string, std::less<>>& story_autosave_rows();

//  Whether `row` saves through the menu at its end: every row but
//  story_autosave_rows().
bool story_row_saves(const Pokemon::StoryCheckpoint& row);

//  How many rows from the first need no action of
//  story_actions_not_implemented(): the part of the story a run can play.
size_t story_rows_ready();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
