/*  BDSP AutoStory Planned Route Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_PlannedRoute_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_PlannedRoute_Tests_H

#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  walk_route() on a script of console outcomes (a ScriptedRouteIO): what
//  the walker does after a trainer stops a leg, and when a leg to a wall
//  counts as at its wall.
void add_tests_AutoStory_PlannedRoute(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
