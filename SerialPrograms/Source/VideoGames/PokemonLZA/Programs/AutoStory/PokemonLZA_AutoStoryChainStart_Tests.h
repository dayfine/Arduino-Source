/*  PokemonLZA AutoStory - Chained start check Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryChainStart_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryChainStart_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the tests of run_plza_segments()'s start check on the live
//  screen between two rows, on scripted looks.
void add_tests_AutoStoryChainStart(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
