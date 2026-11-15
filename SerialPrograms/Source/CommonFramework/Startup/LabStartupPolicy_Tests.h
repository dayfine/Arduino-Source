/*  Lab Startup Tests
 *
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_LabStartupPolicy_Tests_H
#define PokemonAutomation_LabStartupPolicy_Tests_H
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{

// Register lab-policy and startup-dispatch regressions; no Qt or hardware is opened.
void add_tests_LabStartupPolicy(UnitTestDatabase& database);

} // namespace PokemonAutomation
#endif
