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

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for story event sequencing and execution.
void add_tests_StoryEvents(UnitTestDatabase& database);
// Registers unit tests for story segment running, verification, and branching.
void add_tests_StorySegments(UnitTestDatabase& database);
// Registers unit tests for high-level story step dispatching and table conversion.
void add_tests_StorySteps(UnitTestDatabase& database);
// Registers unit tests for the lossless shared story-data wire contract.
void add_tests_StoryStepJson(UnitTestDatabase& database);
// Registers unit tests for step args: action specs and reading old variant strings.
void add_tests_StoryArgs(UnitTestDatabase& database);
// Registers unit tests for the shared authored envelope, game identity and embedded-resource loading.
void add_tests_AuthoredCheckpointJson(UnitTestDatabase& database);
// Registers unit tests for the shared checkpoint log lines and stats counts.
void add_tests_StoryLog(UnitTestDatabase& database);
// Registers unit tests for the shared start/end checkpoint range options.
void add_tests_StoryRangeOptions(UnitTestDatabase& database);

// Registers every suite above, for the unit test runner.
inline void add_tests_AutoStory(UnitTestDatabase& database){
    add_tests_StoryEvents(database);
    add_tests_StorySegments(database);
    add_tests_StorySteps(database);
    add_tests_StoryStepJson(database);
    add_tests_StoryArgs(database);
    add_tests_AuthoredCheckpointJson(database);
    add_tests_StoryLog(database);
    add_tests_StoryRangeOptions(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
