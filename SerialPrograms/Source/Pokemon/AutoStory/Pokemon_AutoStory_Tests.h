/*  AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the pieces every AutoStory program shares.
 *  add_tests_AutoStory() is the one entry point the unit test runner calls;
 *  a new shared AutoStory suite registers there.
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStory_Tests_H
#define PokemonAutomation_Pokemon_AutoStory_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/Pokemon_StoryCheckpointJson_Tests.h"
#include "Engine/Pokemon_ScreenMatch_Tests.h"
#include "Engine/Pokemon_StoryBackend_Tests.h"
#include "Engine/Pokemon_StoryEngine_Tests.h"
#include "Engine/Pokemon_StoryLaunch_Tests.h"
#include "Engine/Pokemon_StorySavePolicy_Tests.h"
#include "Engine/Pokemon_StoryVerifyRuns_Tests.h"
#include "Grid/Pokemon_GridStoryActions_Tests.h"
#include "Options/Pokemon_StoryRangeOptions_Tests.h"
#include "Pokemon_AutoStorySelect_Tests.h"
#include "Pokemon_CenterVisit_Tests.h"
#include "Pokemon_FrameReplay_Tests.h"
#include "Pokemon_Interrupts_Tests.h"
#include "Pokemon_SceneJudges_Tests.h"
#include "Pokemon_ScriptedReadings_Tests.h"
#include "Pokemon_Settle_Tests.h"
#include "Pokemon_StoryFailureClasses_Tests.h"
#include "Pokemon_StoryInterrupts_Tests.h"
#include "Pokemon_StoryLog_Tests.h"
#include "Pokemon_StoryRngHandoff_Tests.h"
#include "Pokemon_Transitions_Tests.h"
#include "Types/Pokemon_StoryCheckLevel_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers every shared AutoStory suite, for the unit test runner.
inline void add_tests_AutoStory(UnitTestDatabase& database){
    add_tests_AutoStorySelect(database);
    add_tests_StoryRangeOptions(database);
    add_tests_StoryLog(database);
    add_tests_StoryCheckpointJson(database);
    add_tests_ScreenMatch(database);
    add_tests_StoryBackend(database);
    add_tests_StoryEngine(database);
    add_tests_GridStoryActions(database);
    add_tests_Transitions(database);
    add_tests_Interrupts(database);
    add_tests_StoryInterrupts(database);
    add_tests_SceneJudges(database);
    add_tests_Settle(database);
    add_tests_CenterVisit(database);
    add_tests_StoryCheckLevel(database);
    add_tests_StorySavePolicy(database);
    add_tests_StoryVerifyRuns(database);
    add_tests_StoryFailureClasses(database);
    add_tests_StoryLaunch(database);
    add_tests_StoryRngHandoff(database);
    add_tests_ScriptedReadings(database);
    add_tests_FrameReplay(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
