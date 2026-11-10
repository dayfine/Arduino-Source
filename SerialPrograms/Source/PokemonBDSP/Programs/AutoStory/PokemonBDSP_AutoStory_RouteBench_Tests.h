/*  BDSP AutoStory - Route Bench Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteBench_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteBench_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The route bench: every story table route's plan scored, against a golden file.
void add_tests_AutoStory_RouteBench(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
