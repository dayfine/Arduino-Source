/*  AutoStory Fault Sweep Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFaultSweep_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFaultSweep_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Each class of console failure injected at every row of the story on the
//  overworld sim; the rows the program still fails are known failing
//  (PokemonFRLG_AutoStoryFaultSweep_Tests.cpp).
void add_tests_AutoStoryFaultSweep(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
