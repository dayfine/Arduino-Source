/*  Story Engine Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEnginePolicy_Tests_H
#define PokemonAutomation_Pokemon_StoryEnginePolicy_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the engine's save decision, postconditions, steps hash, metadata and observation screens.
void add_tests_StoryEnginePolicy(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
