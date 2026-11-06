/*  BBQ Diagnostic Logger Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQLogger_Tests_H
#define PokemonAutomation_PokemonSV_BBQLogger_Tests_H

#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

// Registers deterministic event formatting and ordering tests; no console input.
void add_tests_BBQDiagnosticLogs(UnitTestDatabase& database);

} // namespace PokemonSV
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
