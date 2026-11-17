/*  AutoStory Row Tests, Segment 20
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registration of the tests that run segment 20's rows (the Pokemon
 *  League: the Elite Four and the Champion) on the overworld sim.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRows20_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRows20_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Rows 20.01a to 20.05 on the overworld sim (PokemonFRLG_AutoStoryLeague_Tests.cpp).
void add_tests_AutoStoryLeague(UnitTestDatabase& database);

//  The budgeted C2 policy's turns in the League's battles
//  (PokemonFRLG_AutoStoryLeaguePlan_Tests.cpp).
void add_tests_AutoStoryLeaguePlan(UnitTestDatabase& database);

//  The League's fight rows through the engine's verification loop, and the
//  top-up before each (PokemonFRLG_AutoStoryLeagueRows_Tests.cpp).
void add_tests_AutoStoryLeagueRows(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
