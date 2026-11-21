/*  Story Files Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFiles_Tests_H
#define PokemonAutomation_Pokemon_StoryFiles_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Registers the tests of a story read from built-in and dev checkpoint
//  files, and of the shared execution fields.
void add_tests_StoryFiles(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
