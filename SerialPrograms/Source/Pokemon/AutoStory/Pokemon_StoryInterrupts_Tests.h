/*  Story Interrupts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryInterrupts_Tests_H
#define PokemonAutomation_Pokemon_StoryInterrupts_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the table executor's looks for interrupts between steps.
void add_tests_StoryInterrupts(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
