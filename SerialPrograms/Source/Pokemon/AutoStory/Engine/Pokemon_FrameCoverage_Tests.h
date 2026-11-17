/*  Frame Coverage Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_FrameCoverage_Tests_H
#define PokemonAutomation_Pokemon_FrameCoverage_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for judging a watch window on the frames it inspected.
void add_tests_FrameCoverage(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
