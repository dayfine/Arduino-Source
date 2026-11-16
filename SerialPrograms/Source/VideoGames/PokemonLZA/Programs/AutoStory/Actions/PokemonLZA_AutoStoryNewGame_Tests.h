/*  PokemonLZA AutoStory - New game Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryNewGame_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryNewGame_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the new game's tests: the choices' checks, the language list's
//  and look picker's moves, the name's key presses and capture mode's
//  recording and file names.
void add_tests_AutoStoryNewGame(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
