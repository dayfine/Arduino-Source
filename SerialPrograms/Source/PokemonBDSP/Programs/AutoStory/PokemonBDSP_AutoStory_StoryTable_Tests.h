/*  BDSP AutoStory - StoryTable Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Tests that need no console: the story table: rows lint, every step's variant is known, and the walks its events plan.
void add_tests_AutoStory_StoryTable(UnitTestDatabase& database);



}
}
}
#endif
