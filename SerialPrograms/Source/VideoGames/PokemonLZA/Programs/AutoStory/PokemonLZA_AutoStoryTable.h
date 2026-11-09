/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story as rows of the shared story steps
 *  (Pokemon/AutoStory/Pokemon_StorySteps.h), plain data as BDSP's and
 *  FRLG's tables are, so unit tests can check it without a console.
 *
 *  Lumiose has no tile grid, so a row's poses name a place and nothing more,
 *  and a walk in the overworld is a GAME step whose variant names one of
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
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "PokemonLZA_AutoStoryActions.h"
#include "PokemonLZA_AutoStoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The story's checkpoints in order: 01.xx Main Mission 01 (new game,
//  Pancham, the starter), 02.xx Main Mission 02 (out of the Battle Zone to
//  Hotel Z), 03.xx Main Mission 03 (Taunie's battle, the Pokemon Center,
//  Wild Zone 1, Mable's Rock Smash TM). Only 01.00, a new game, starts
//  without a save.
const std::vector<Pokemon::StoryCheckpoint>& story_checkpoints();

//  Whether `row` saves through the menu at its end. Rows before the game
//  has its menu (01.00-01.02) don't: the game autosaves before the starter,
//  and that autosave is 01.03's start.
bool story_row_saves(const Pokemon::StoryCheckpoint& row);

//  The overworld walks the table's GAME steps name, by name.
const std::map<std::string, StoryMove, std::less<>>& story_moves();

//  The walk to each starter, for the "approach_starter" step; a starter
//  without one was not recorded.
const std::map<StarterChoice, StoryMove>& starter_moves();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
