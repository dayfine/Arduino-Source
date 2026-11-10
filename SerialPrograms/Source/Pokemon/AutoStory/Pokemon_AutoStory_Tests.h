/*  AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the pieces every AutoStory program shares, story
 *  engine or not. add_tests_AutoStory() is the one entry point the unit
 *  test runner calls; a new shared AutoStory suite registers there.
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStory_Tests_H
#define PokemonAutomation_Pokemon_AutoStory_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Options/Pokemon_StoryRangeOptions_Tests.h"
#include "Pokemon_AutoStorySelect_Tests.h"
#include "Pokemon_StoryStepExecutor_Tests.h"
#include "Data/Pokemon_AuthoredCheckpointJson_Tests.h"
#include "Data/Pokemon_StoryStepJson_Tests.h"
#include "Types/Pokemon_StoryActionArg_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the shared checkpoint log lines.
void add_tests_StoryLog(UnitTestDatabase& database);
// Registers unit tests for the story counts added to the AutoStory stats.
void add_tests_AutoStoryStats(UnitTestDatabase& database);
// Registers unit tests for story event sequencing and execution.
void add_tests_StoryEvents(UnitTestDatabase& database);
// Registers unit tests for story segment running, verification, and branching.
void add_tests_StorySegments(UnitTestDatabase& database);

// Registers every shared AutoStory suite, for the unit test runner.
inline void add_tests_AutoStory(UnitTestDatabase& database){
    add_tests_AutoStorySelect(database);
    add_tests_AutoStoryStats(database);
    add_tests_StoryLog(database);
    add_tests_StoryRangeOptions(database);
    add_tests_StoryEvents(database);
    add_tests_StorySegments(database);
    add_tests_StoryStepExecutor(database);
    add_tests_StoryStepJson(database);
    add_tests_StoryActionArg(database);
    add_tests_AuthoredCheckpointJson(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
