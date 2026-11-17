/*  BDSP AutoStory Simulated Route Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_SimulatedRoute_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_SimulatedRoute_Tests_H

#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  walk_route() on a simulated console (SimulatedRouteIO): the Jubilife
//  Center walks with overshooting holds and people in the way, and a fault
//  sweep over every ROUTE step of the story table.
void add_tests_AutoStory_SimulatedRoute(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
