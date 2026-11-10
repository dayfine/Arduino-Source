/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story as rows of the shared story steps
 *  (Pokemon/AutoStory/Types/Pokemon_StoryStep.h), plain data as BDSP's and
 *  FRLG's tables are, so unit tests can check it without a console.
 *
 *  Lumiose has no tile grid, so a row's poses name a place and nothing more,
 *  and a walk in the overworld is a GAME step whose action names one of
 *  story_moves(): legs of a relative turn and a walk time.
 *
 *  Recorded 2026-11-08/09 on console B, played step by step from a new
 *  game to the quest "Return to Hotel Z to meet up with Taunie" in Main
 *  Mission 03. Every walk was measured walking, not dashing, and none has
 *  run as a story step yet. Steps the game has no code for yet are GAME
 *  steps of story_actions_not_implemented().
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTable_H
#define PokemonAutomation_PokemonLZA_AutoStoryTable_H

#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_ScreenEventTypes.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The checked-in checkpoint files, StoryData/plza-<id>.plan.json, built
//  into the program (cmake/EmbedTextFiles.cmake): (name, text), by name.
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  The story's checkpoints in order, read from autostory_checkpoint_json()
//  against plza_story_specs(): 01.xx Main Mission 01 (new game, Pancham,
//  the starter), 02.01 Main Mission 02 (out of the Battle Zone to Hotel Z,
//  the first night), 03.xx Main Mission 03 (Taunie's battle, the Pokemon
//  Center, Wild Zone 1, Mable's Rock Smash TM). Only 01.00, a new game,
//  starts without a save; it starts on the title screen.
const std::vector<Pokemon::StoryCheckpoint>& story_checkpoints();

//  The C++ rows the JSON files were written from, for the parity test.
//  Removed once it passes.
const std::vector<Pokemon::StoryCheckpoint>& story_checkpoints_cpp();

//  The rows that end on one of the game's autosaves and do not save
//  themselves (their JSON "execution": {"save": "autosave"}): before 03.02
//  the main menu cannot save. Each ends where the game autosaved in the
//  recording (before the starter, the Main Mission 02 and 03 banners, the
//  Hotel Z lobby scene), so the next row reloads there.
const std::set<std::string, std::less<>>& story_autosave_rows();

//  Whether `row` saves through the menu at its end: every row but
//  story_autosave_rows().
bool story_row_saves(const Pokemon::StoryCheckpoint& row);

//  How many rows from the first need no action of
//  story_actions_not_implemented(): the part of the story a run can play.
size_t story_rows_ready();

//  The overworld walks the table's GAME steps name, by name.
const std::map<std::string, StoryMove, std::less<>>& story_moves();

//  The moves of story_moves() that end on a screen event: the walk holds its
//  last leg until the event shows, the leg's time as the upper bound
//  (walk_until()). Every other move walks its times.
const std::map<std::string, Pokemon::ScreenEvent, std::less<>>& story_move_ends();

//  The walk to each starter, for the "approach_starter" step; a starter
//  without one was not recorded.
const std::map<StarterChoice, StoryMove>& starter_moves();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
