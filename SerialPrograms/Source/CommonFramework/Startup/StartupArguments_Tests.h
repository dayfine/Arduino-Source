/*  Lab Startup Tests
 *
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_StartupArguments_Tests_H
#define PokemonAutomation_StartupArguments_Tests_H
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{

// Register parser and startup-dispatch regressions; no Qt or hardware is opened.
void add_tests_StartupArguments(UnitTestDatabase& database);

} // namespace PokemonAutomation
#endif
