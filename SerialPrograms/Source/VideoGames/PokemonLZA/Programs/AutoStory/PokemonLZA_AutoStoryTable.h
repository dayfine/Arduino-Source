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
//  first step is the GAME action "new_game_intro", from HOME: 01.00a; or
//  "confirm_new_game", on "Confirm Choices": 01.00b).
bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row);

//  Whether `row` plays the new game's choices (its first step is the GAME
//  action "new_game_intro"): 01.00a, which types the name.
bool story_row_makes_new_game_choices(const Pokemon::StoryCheckpoint& row);




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
