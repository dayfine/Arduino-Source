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
#include "Data/Pokemon_AuthoredCheckpointJson_Tests.h"
#include "Data/Pokemon_StoryFiles_Tests.h"
#include "Data/Pokemon_StoryStepJson_Tests.h"
#include "Engine/Pokemon_StoryBackend_Tests.h"
#include "Engine/Pokemon_StoryEngine_Tests.h"
#include "Pokemon_CheckpointAttempts_Tests.h"
#include "Pokemon_StoryEvents_Tests.h"
#include "Pokemon_StoryLog_Tests.h"
#include "Types/Pokemon_StoryActionArg_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers every shared AutoStory suite, for the unit test runner.
inline void add_tests_AutoStory(UnitTestDatabase& database){
    add_tests_StoryActionArg(database);
    add_tests_StoryLog(database);
    add_tests_StoryStepJson(database);
    add_tests_AuthoredCheckpointJson(database);
    add_tests_StoryFiles(database);
    add_tests_StoryBackend(database);
    add_tests_CheckpointAttempts(database);
    add_tests_StoryEvents(database);
    add_tests_StoryEngine(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
