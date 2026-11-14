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
#include "Engine/Pokemon_StoryEnginePolicy_Tests.h"
#include "Engine/Pokemon_StoryEngineRuns_Tests.h"
#include "Engine/Pokemon_StoryEngine_Tests.h"
#include "Pokemon/Options/Pokemon_StoryRangeOptions_Tests.h"
#include "Pokemon_AutoStorySelect_Tests.h"
#include "Pokemon_CheckpointRetry_Tests.h"
#include "Pokemon_Interrupts_Tests.h"
#include "Pokemon_SceneJudges_Tests.h"
#include "Pokemon_StoryActions_Tests.h"
#include "Pokemon_StoryEvents_Tests.h"
#include "Pokemon_StoryInterrupts_Tests.h"
#include "Pokemon_StoryLog_Tests.h"
#include "Pokemon_StoryRngHandoff_Tests.h"
#include "Pokemon_StoryRunCounts_Tests.h"
#include "Pokemon_StoryRun_Tests.h"
#include "Pokemon_StorySavePolicy_Tests.h"
#include "Pokemon_StorySegments_Tests.h"
#include "Pokemon_StoryStepExecutor_Tests.h"
#include "Pokemon_Transitions_Tests.h"
#include "Types/Pokemon_StoryActionArg_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers every shared AutoStory suite, for the unit test runner.
inline void add_tests_AutoStory(UnitTestDatabase& database){
    add_tests_AutoStorySelect(database);
    add_tests_StoryRun(database);
    add_tests_StoryRunCounts(database);
    add_tests_CheckpointRetry(database);
    add_tests_StoryLog(database);
    add_tests_StoryRngHandoff(database);
    add_tests_StoryRangeOptions(database);
    add_tests_StoryEvents(database);
    add_tests_StorySegments(database);
    add_tests_StoryStepExecutor(database);
    add_tests_StoryStepJson(database);
    add_tests_StoryActionArg(database);
    add_tests_AuthoredCheckpointJson(database);
    add_tests_StoryFiles(database);
    add_tests_StorySavePolicy(database);
    add_tests_StoryEngine(database);
    add_tests_StoryEnginePolicy(database);
    add_tests_StoryEngineRuns(database);
    add_tests_SceneJudges(database);
    add_tests_Interrupts(database);
    add_tests_Transitions(database);
    add_tests_StoryActions(database);
    add_tests_StoryInterrupts(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
