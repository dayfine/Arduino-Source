/*  Story Step Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the story steps: their executor, wire format, args
 *  and authored envelope. add_tests_AutoStory() registers them.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStep_Tests_H
#define PokemonAutomation_Pokemon_StoryStep_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for high-level story step dispatching and table conversion.
void add_tests_StoryStepExecutor(UnitTestDatabase& database);
// Registers unit tests for the lossless shared story-data wire contract.
void add_tests_StoryStepJson(UnitTestDatabase& database);
// Registers unit tests for step args: checking them against an action's spec.
void add_tests_StoryActionArg(UnitTestDatabase& database);
// Registers unit tests for the shared authored envelope, game identity and embedded-resource loading.
void add_tests_AuthoredCheckpointJson(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
