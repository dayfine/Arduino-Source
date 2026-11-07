/*  BDSP Recovery Walk Tests
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RecoveryWalk_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RecoveryWalk_Tests_H
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
//  Register mine interruption, localization and bounded recovery scenarios.
void add_tests_AutoStory_RecoveryWalk(UnitTestDatabase& database);
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
