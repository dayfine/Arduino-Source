/*  PokemonLZA AutoStory - Minimap match Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMinimapMatch_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryMinimapMatch_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers matching_minimap()'s tests, on screens painted from the
//  built-in 03.04 start reference: brighter, darker and moved; and
//  minimap_reference_json()'s, a start recorded as a reference.
void add_tests_AutoStoryMinimapMatch(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
