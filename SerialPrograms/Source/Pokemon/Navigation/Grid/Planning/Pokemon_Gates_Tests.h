/*  Gates Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_Gates_Tests_H
#define PokemonAutomation_Pokemon_Gates_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Registers the gate tests: the map reader's gates, switches and one-way
//  warps, the gate state, and the gated planner.
void add_tests_Gates(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
