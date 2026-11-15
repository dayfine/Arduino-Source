/*  PokemonLZA AutoStory - Story game Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGame_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryGame_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the story game's tests: the built-in checkpoint files load and
//  pass the lint, and their steps run on a scripted backend (the opening's
//  platform wait, the walk off the platform into the white fade, the
//  arrival scenes to free control).
void add_tests_AutoStoryGame(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
