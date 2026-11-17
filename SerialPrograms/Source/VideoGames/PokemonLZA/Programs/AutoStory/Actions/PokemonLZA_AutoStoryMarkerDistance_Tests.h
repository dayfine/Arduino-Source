/*  PokemonLZA AutoStory - Marker distance Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerDistance_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerDistance_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers MarkerDistance's tests: walks to a stop replayed from the
//  marker's distance reads, one look and one step at a time.
void add_tests_AutoStoryMarkerDistance(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
