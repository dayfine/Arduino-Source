/*  PokemonLZA AutoStory - Story backend Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackend_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackend_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers face_heading()'s tests: turns to a heading from any start, on
//  a game whose arrow shows a turn only once it is done.
void add_tests_AutoStoryBackend(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
