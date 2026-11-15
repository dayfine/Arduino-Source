/*  Screen Machine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenMachine_ScreenMachine_Tests_H
#define PokemonAutomation_CommonTools_ScreenMachine_ScreenMachine_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{


//  Registers the screen machine's tests: scripted observations, no frames
//  from disk.
void add_tests_ScreenMachine(UnitTestDatabase& database);



}  // namespace PokemonAutomation
#endif
