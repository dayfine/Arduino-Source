/*  PokemonLZA AutoStory - Fault Sweep Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryFaultSweep_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryFaultSweep_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The shared fault sweep (Pokemon_StoryFaultSweep.h) over PLZA's rows, on
//  a row sim of the steps that need no console, and that a PLZA step's
//  failure line carries its class.
void add_tests_AutoStoryFaultSweep(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
