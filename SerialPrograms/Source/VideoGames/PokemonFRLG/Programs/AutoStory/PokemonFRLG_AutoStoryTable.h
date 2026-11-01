/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FireRed from a new game to the Boulder Badge, as story checkpoints
 *  00.01 to 00.09, each saved at its end. Taken from a by-hand run on
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


//  Checkpoints 00.01 to 00.09, in order.
const std::vector<StoryCheckpoint>& story_checkpoints();


void add_tests_AutoStory(UnitTestDatabase& database);



}
}
}
#endif
