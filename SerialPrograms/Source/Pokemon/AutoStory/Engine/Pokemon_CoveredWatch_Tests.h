/*  Covered Watch Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_CoveredWatch_Tests_H
#define PokemonAutomation_Pokemon_CoveredWatch_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the watcher wrapper that records the frames a watch inspected.
void add_tests_CoveredWatch(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
