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
 *  This is the story as data, with no IO: the checkpoint files it is read
 *  from, built in or from the dev folder, are PokemonLZA_AutoStoryStoryFiles.h.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTable_H
#define PokemonAutomation_PokemonLZA_AutoStoryTable_H

#include <vector>
#include "Pokemon/AutoStory/Data/Pokemon_StoryFiles.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The story a run plays: its checkpoints in order, and the rows their
//  execution marks (Pokemon_StoryRowExecution.h): the autosave rows, which
//  do not save themselves ("save": "autosave"), and the rows a run reloads
//  before ("reload_when_chained").
using StoryData = Pokemon::StoryData<Pokemon::StoryCheckpoint>;

//  Whether `row` of `story` saves through the menu at its end: every row
//  but the autosave rows. The menu (X, then R) can save from the start of
//  the story too: 01.03 saves through it after Andi (console B, 2026-11-10).
bool story_row_saves(const StoryData& story, const Pokemon::StoryCheckpoint& row);

//  How many rows of `checkpoints` from the first need no action of
//  story_actions_not_implemented(): the part of the story a run can play.
size_t story_rows_ready(const std::vector<Pokemon::StoryCheckpoint>& checkpoints);

//  Whether `row` starts with no save to load: it starts on the new game's
//  "Confirm Choices" screen, the language, look and name chosen by hand
//  (its first step is the GAME action "confirm_new_game"): 01.00.
bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
