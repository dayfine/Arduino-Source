/*  AutoStory Row Tests, Segment 19
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registration of the tests that run segment 19's rows (Route 22's rival,
 *  Route 23, Victory Road) on the overworld sim.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRows19_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRows19_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Rows 19.01 and after on the overworld sim (PokemonFRLG_AutoStoryVictoryRoad_Tests.cpp).
void add_tests_AutoStoryVictoryRoad(UnitTestDatabase& database);
//  The boulder solver and the sim's boulders (PokemonFRLG_AutoStoryBoulders_Tests.cpp).
void add_tests_AutoStoryBoulders(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
