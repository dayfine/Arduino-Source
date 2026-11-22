/*  BDSP Blink Recovery Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_BlinkRecovery_Tests_H
#define PokemonAutomation_PokemonBDSP_BlinkRecovery_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Registers unit tests for blink frame-gap guarding and confirmation recovery.
void add_tests_BlinkRecovery(UnitTestDatabase& database);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
