/*  PokemonLZA AutoStory - Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story as rows of the shared story steps, plain data with no IO.
 *  Lumiose has no tile grid: poses name a place only, and a walk is a GAME
 *  "walk" step whose args hold timed legs (PokemonLZA_AutoStoryGame.h).
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


//  The story's checkpoints in order.
using StoryData = Pokemon::StoryData<Pokemon::StoryCheckpoint>;

//  A new game: 01.00a (from HOME) or 01.00b (on "Confirm Choices").
bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row);

//  01.00a, which types the name.
bool story_row_makes_new_game_choices(const Pokemon::StoryCheckpoint& row);

//  "<step>: <rule>" for each step of `row` not right after the step it needs:
//  a TALK after a walk ending on an A prompt; a "story_battle" after
//  "story_battle_start"; a scene ("a", MASH) after what starts it (a walk,
//  talk, choice or battle), not after a step that presses through to
//  control, which would play the scene itself. A bare "mash" may follow anything.
std::vector<std::string> story_row_lead_in_errors(const Pokemon::StoryCheckpoint& row);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
