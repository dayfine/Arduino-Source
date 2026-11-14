/*  AutoStory Story Run Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRun_Tests_H
#define PokemonAutomation_Pokemon_StoryRun_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the AutoStory story run's start, reload and finish.
void add_tests_StoryRun(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
