/*  AutoStory Row Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registration of the tests that run story rows on the overworld sim,
 *  one file per stretch of the story.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRows_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRows_Tests_H

#include <string_view>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Rows 05.10 and 06.01 on the overworld sim (PokemonFRLG_AutoStoryVermilion_Tests.cpp).
void add_tests_AutoStoryVermilion(UnitTestDatabase& database);
//  Rows 06.02 to 06.04 on the overworld sim (PokemonFRLG_AutoStoryVermilionGym_Tests.cpp).
void add_tests_AutoStoryVermilionGym(UnitTestDatabase& database);
//  Rows from 07.01 on on the overworld sim (PokemonFRLG_AutoStoryCeladon_Tests.cpp),
//  with add_tests_AutoStoryLavender() and add_tests_AutoStoryTower().
void add_tests_AutoStoryCeladon(UnitTestDatabase& database);
//  Rows from 08.01 on on the overworld sim (PokemonFRLG_AutoStoryLavender_Tests.cpp).
void add_tests_AutoStoryLavender(UnitTestDatabase& database);
//  Rows 10.01 to 10.06 on the overworld sim (PokemonFRLG_AutoStoryTower_Tests.cpp).
void add_tests_AutoStoryTower(UnitTestDatabase& database);
//  Row 11.01 on the overworld sim (PokemonFRLG_AutoStorySnorlax_Tests.cpp).
void add_tests_AutoStorySnorlax(UnitTestDatabase& database);
//  Row 11.02 on the sims (PokemonFRLG_AutoStorySlope_Tests.cpp).
void add_tests_AutoStorySlope(UnitTestDatabase& database);
//  Row 11.03 on the sims (PokemonFRLG_AutoStorySafari_Tests.cpp).
void add_tests_AutoStorySafari(UnitTestDatabase& database);

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
