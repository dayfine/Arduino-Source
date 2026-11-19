/*  Lab Device Access Tests
 *
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_LabDeviceAccessQt_Tests_H
#define PokemonAutomation_LabDeviceAccessQt_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{

// Register offline policy, lifecycle and cross-process lease regression cases.
void add_tests_LabDeviceAccess(UnitTestDatabase& database);

} // namespace PokemonAutomation
#endif
