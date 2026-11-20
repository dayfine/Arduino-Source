/*  PokemonLZA AutoStory - Wild Zone 1 Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the tests of 03.08's walk plans in Wild Zone 1: out of the
//  zone, the start off Taunie's rocks (ZoneStart) and the map legs (MapLeg).
void add_tests_AutoStoryZone1(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
