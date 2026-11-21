/*  Interrupts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_Interrupts_Tests_H
#define PokemonAutomation_Pokemon_Interrupts_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the timed screen-event log, the warp judge and the interrupt classifier.
void add_tests_Interrupts(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
