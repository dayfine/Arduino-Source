/*  PokemonLZA AutoStory - Wild Zone 1 map Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1Map_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1Map_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers zone_1_exit_via()'s and check_out_of_zone_1()'s tests, the
//  latter on scripted map pin reads.
void add_tests_AutoStoryZone1Map(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
