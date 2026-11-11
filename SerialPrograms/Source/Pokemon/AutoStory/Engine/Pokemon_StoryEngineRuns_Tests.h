/*  Story Engine Runs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEngineRuns_Tests_H
#define PokemonAutomation_Pokemon_StoryEngineRuns_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for what games need of the engine: any pose type, the stick, observation machines, segment and verify runs, console-only actions.
void add_tests_StoryEngineRuns(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
