/*  BDSP AutoStory Story Simulation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StorySimulation_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StorySimulation_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Segments 00 and 01 run end to end without a console: the story table,
//  the shared segment runner and its retry policy, against a fake game
//  that plans every route and walk on the built-in maps.
void add_tests_AutoStory_StorySimulation(UnitTestDatabase& database);



}
}
}
#endif
