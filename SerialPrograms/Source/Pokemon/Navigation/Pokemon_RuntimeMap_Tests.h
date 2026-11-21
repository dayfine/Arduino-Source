/*  Versioned Runtime Map Tests
 *
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_RuntimeMap_Tests_H
#define PokemonAutomation_RuntimeMap_Tests_H
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{

// Register wire-schema compatibility/rejection and full normalized-value checks.
void add_tests_RuntimeMap(UnitTestDatabase& database);

} // namespace Pokemon
} // namespace PokemonAutomation
#endif
