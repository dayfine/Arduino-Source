/*  Story Save Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySavePolicy_Tests_H
#define PokemonAutomation_Pokemon_StorySavePolicy_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for when a run saves (save modes) and the postcondition check.
void add_tests_StorySavePolicy(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
