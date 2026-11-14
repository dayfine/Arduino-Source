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

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Rows 00.21 and 00.22 on the overworld sim (PokemonFRLG_AutoStoryVermilion_Tests.cpp).
void add_tests_AutoStoryVermilion(UnitTestDatabase& database);
//  Rows 00.23 to 00.25 on the overworld sim (PokemonFRLG_AutoStoryVermilionGym_Tests.cpp).
void add_tests_AutoStoryVermilionGym(UnitTestDatabase& database);
//  Rows from 00.26 on on the overworld sim (PokemonFRLG_AutoStoryCeladon_Tests.cpp).
void add_tests_AutoStoryCeladon(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
