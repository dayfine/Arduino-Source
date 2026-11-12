/*  BDSP AutoStory - Sim golden tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_SimGolden_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_SimGolden_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  What the story simulation does with each row, clean and under the fault
//  sweep, against the goldens beside the tests
//  (PokemonBDSP_AutoStory_SimGolden_Tests.cpp).
void add_tests_AutoStory_SimGolden(UnitTestDatabase& database);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
