/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Segments 00 and 01, new game to Floaroma, as rows of the shared story
 *  steps (Pokemon/Navigation/Pokemon_StorySteps.h): plain data, as FRLG's
 *  table is, so the tests can plan its routes and run it against a fake
 *  game. Each row saves at its start, which is where the row before it
 *  ends.
 *
 *  Steps that have no declarative form yet call one of the old functions:
 *  a GAME step (or another kind with a variant) names it, and the segment
 *  registers it with its executor (segment_00_variants(),
 *  segment_01_variants()). Those are the next to split.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_H

#include <vector>
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryTables.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  00.00 to 00.05: the settings, the lake, the Pokedex, the catch, Sandgem
//  and Route 202 (00.04b to 00.04d as story steps), Jubilife to the Poketch.
const std::vector<StoryRow>& segment_00_table();

//  01.01 to 01.11: Jubilife to Oreburgh and the Coal Badge, back, and north
//  to Floaroma's old couple.
const std::vector<StoryRow>& segment_01_table();



}
}
}
#endif
