/*  AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the shared story engine (Pokemon/AutoStory).
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStory_Tests_H
#define PokemonAutomation_Pokemon_AutoStory_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon_ScreenStory_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Recorded frames replayed through the detectors on a virtual clock.
void add_tests_FrameReplay(UnitTestDatabase& database);
// Registers unit tests for story event sequencing and execution.
void add_tests_StoryEvents(UnitTestDatabase& database);
// Registers unit tests for story segment running, verification, and branching.
void add_tests_StorySegments(UnitTestDatabase& database);
// Registers unit tests for high-level story step dispatching and table conversion.
void add_tests_StoryStepExecutor(UnitTestDatabase& database);
// Registers unit tests for the lossless shared story-data wire contract.
void add_tests_StoryStepJson(UnitTestDatabase& database);
// Registers unit tests for step args: checking them against an action's spec.
void add_tests_StoryActionArg(UnitTestDatabase& database);
// Registers unit tests for the shared authored envelope, game identity and embedded-resource loading.
void add_tests_AuthoredCheckpointJson(UnitTestDatabase& database);

// Registers the story engine's suites above (StoryEvents and StorySegments are
// registered on their own), for the unit test runner.
inline void add_tests_AutoStory(UnitTestDatabase& database){
    add_tests_StoryStepExecutor(database);
    add_tests_StoryStepJson(database);
    add_tests_StoryActionArg(database);
    add_tests_AuthoredCheckpointJson(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
