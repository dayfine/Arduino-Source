/*  PokemonLZA AutoStory - Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story as rows of the shared story steps
 *  (Pokemon/AutoStory/Types/Pokemon_StoryStep.h), plain data, so unit
 *  tests can check it without a console.
 *
 *  Lumiose has no tile grid, so a row's poses name a place and nothing more,
 *  and a walk in the overworld is a GAME "walk" step whose args hold its
 *  legs: a relative turn and a walk time each (PokemonLZA_AutoStoryGame.h).
 *
 *  This is the story as data, with no IO: the checkpoint files it is read
 *  from are PokemonLZA_AutoStoryStoryFiles.h.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTable_H
#define PokemonAutomation_PokemonLZA_AutoStoryTable_H

#include <string>
#include <vector>
#include "Pokemon/AutoStory/Data/Pokemon_StoryRowExecution.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The story a run plays: its checkpoints in order, and the rows their
//  execution marks (Pokemon_StoryRowExecution.h).
using StoryData = Pokemon::StoryData<Pokemon::StoryCheckpoint>;

//  Whether `row` starts with no save to load: it starts a new game (its
//  first step is the GAME action "new_game_intro", or "confirm_new_game"
//  on "Confirm Choices"): 01.00.
bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row);

//  "<step>: <rule>" for each step of `row` that does not come right after
//  the step it needs (console B, 2026-11-16):
//    - a TALK after a step that ends on an A prompt (a "walk" to "end"
//      "prompt", or "walk_to_prompt"): a timed walk can stop short of who
//      the talk is for (03.02);
//    - a "story_battle" after "story_battle_start", which presses through
//      the talk and cards to the battle HUD: the battle code run on the
//      overworld looks for a battle for 2 min (03.02b).
std::vector<std::string> story_row_lead_in_errors(const Pokemon::StoryCheckpoint& row);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
