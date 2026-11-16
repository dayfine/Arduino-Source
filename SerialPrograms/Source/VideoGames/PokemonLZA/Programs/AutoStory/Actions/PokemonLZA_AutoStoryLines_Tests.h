/*  PokemonLZA AutoStory - Lines Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryLines_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryLines_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers press_a_through_lines()'s and press_b_out_of_menu()'s tests,
//  on a FakeStoryBackend's scripted looks.
void add_tests_AutoStoryLines(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
