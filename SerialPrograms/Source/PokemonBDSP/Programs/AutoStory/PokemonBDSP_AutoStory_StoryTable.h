/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story from a new game to Floaroma, 00.00 to 01.11, as rows of the
 *  shared story steps (Pokemon/AutoStory/Pokemon_StorySteps.h): plain
 *  data, as FRLG's table is, so the tests plan its routes and run it
 *  against a fake game. Each row but 00.00 saves at its start, which is
 *  where the row before it ends.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_H

#include <vector>
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Checkpoints 00.00 to 01.11, in order: segment 00 (the intro to the
//  Poketch) and segment 01 (Jubilife to Floaroma).
const std::vector<StoryCheckpoint>& story_checkpoints();

//  Mom gives the Running Shoes in this row's step: from then on, B held
//  with the D-pad runs (r24, r27).
constexpr const char* RUNNING_SHOES_ROW = "00.03";
constexpr const char* RUNNING_SHOES_STEP = "rowan_and_dawn";



}
}
}
#endif
