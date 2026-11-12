/*  BDSP AutoStory - Story actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryActions_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryActions_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  The story's actions on the engine (StoryEngineSteps): every step of the
//  story has an action, each runs what the executors' own dispatch ran
//  before the table, and the steps the simulation stubs need the console.
void add_tests_AutoStory_StoryActions(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
