/*  Story Engine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the action registry, the step runner and the
 *  take_warp example (Pokemon/AutoStory/Engine).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEngine_Tests_H
#define PokemonAutomation_Pokemon_StoryEngine_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the action registry, the step runner and the take_warp example.
void add_tests_StoryEngine(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
