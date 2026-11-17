/*  PokemonLZA AutoStory - Marker arrival Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerArrival_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerArrival_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers MarkerArrival's tests: walks to the objective's prompt
//  replayed from console B's logs, one look at a time.
void add_tests_AutoStoryMarkerArrival(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
