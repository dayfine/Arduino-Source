/*  PokemonLZA AutoStory - Zone search Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZoneSearch_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryZoneSearch_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers ZoneSearch's tests: searches replayed look by look, each WALK
//  taking the leg ZoneSearch picks.
void add_tests_AutoStoryZoneSearch(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
