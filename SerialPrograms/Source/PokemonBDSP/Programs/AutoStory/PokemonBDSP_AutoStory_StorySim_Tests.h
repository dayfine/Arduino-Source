/*  BDSP AutoStory Story Sim Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StorySim_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StorySim_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  The story table's rows run by the program's own walking steps
//  (StoryWalkSteps) on the grid simulation: each walk must end where the
//  table says the player is.
void add_tests_AutoStory_StorySim(UnitTestDatabase& database);



}
}
}
#endif
