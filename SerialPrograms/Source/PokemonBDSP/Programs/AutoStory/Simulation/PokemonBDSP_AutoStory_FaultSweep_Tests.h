/*  BDSP AutoStory - Fault sweeps on the story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_FaultSweep_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_FaultSweep_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  Each class of console failure injected at every step of every row where
//  the game can cause it; the rows the program still fails are known failing.
void add_tests_AutoStory_FaultSweep(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
