/*  BDSP AutoStory Story Tables Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryTables_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryTables_Tests_H

#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  Offline checks of the Segment 00 and 01 tables: their rows in order,
//  each checkpoint built and described without a console, none linted.
void add_tests_AutoStory_StoryTables(UnitTestDatabase& database);



}
}
}
#endif
