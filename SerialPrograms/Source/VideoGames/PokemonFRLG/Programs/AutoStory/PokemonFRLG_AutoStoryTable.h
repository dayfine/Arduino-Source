/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FireRed from a new game to the Boulder Badge and the Running Shoes, as
 *  story checkpoints 00.01 to 00.10, each saved at its end. After 00.10
 *  every route and leg runs. Taken from a by-hand run on
 *  console B (2026-11-01) and the pokefirered map scripts.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTable_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTable_H

#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Checkpoints 00.01 to 00.10, in order, with 00.01b, the way back in after
//  SID Helper, after 00.01, and 00.04 split in two at Squirtle's ball for
//  Starter RNG: 00.04 and 00.04b.
const std::vector<StoryCheckpoint>& story_checkpoints();


void add_tests_AutoStory(UnitTestDatabase& database);
//  The table run end to end against a fake game (PokemonFRLG_AutoStorySimulation_Tests.cpp).
void add_tests_AutoStorySimulation(UnitTestDatabase& database);



}
}
}
#endif
