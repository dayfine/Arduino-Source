/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story as rows of the shared story steps
 *  (Pokemon/AutoStory/Pokemon_StorySteps.h), plain data as BDSP's and
 *  FRLG's tables are, so unit tests can check it without a console.
 *
 *  Lumiose has no tile grid, so a row's poses name a place and nothing more,
 *  and a run in the overworld is a GAME step whose variant names one of
 *  story_moves(): a heading and a run time, measured on the console.
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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The story's checkpoints in order. Segment 01 is Main Mission 1 (Pancham
//  takes the travel bag; the starter), from the scaffold's prototype: it has
//  not run on a console.
const std::vector<Pokemon::StoryCheckpoint>& story_checkpoints();

//  The overworld runs the table's GAME steps name, by name.
const std::map<std::string, StoryMove, std::less<>>& story_moves();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
