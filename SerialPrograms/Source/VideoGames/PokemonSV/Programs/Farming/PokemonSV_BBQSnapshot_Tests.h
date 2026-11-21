/*  BBQ Snapshot Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQSnapshot_Tests_H
#define PokemonAutomation_PokemonSV_BBQSnapshot_Tests_H

#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

// Registers local screenshot-write and failure-reporting cases without a console.
void add_tests_BBQSnapshot(UnitTestDatabase& database);

} // namespace PokemonSV
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
