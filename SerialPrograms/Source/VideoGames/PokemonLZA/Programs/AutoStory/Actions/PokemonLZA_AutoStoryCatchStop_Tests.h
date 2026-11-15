/*  PokemonLZA AutoStory - Catch stop Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatchStop_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatchStop_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Tests that need no console: a catch stop's choices (spot visits, what
//  follows a throw, camera pitch resets).
void add_tests_AutoStoryCatchStop(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
