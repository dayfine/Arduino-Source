/*  AutoStory Row Tests, Segment 18
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registration of the tests that run segment 18's rows (Saffron Gym, whose
 *  id is a label: the story plays it after Silph Co.) on the overworld sim,
 *  beside PokemonFRLG_AutoStoryRows16_Tests.h's.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRows18_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRows18_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Rows 18.01 and 18.02, Saffron Gym's pads, on the overworld sim
//  (PokemonFRLG_AutoStorySaffronGym_Tests.cpp).
void add_tests_AutoStorySaffronGym(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
