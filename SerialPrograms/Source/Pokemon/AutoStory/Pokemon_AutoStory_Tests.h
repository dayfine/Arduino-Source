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
#include "Pokemon_StoryFailureClasses_Tests.h"
#include "Pokemon_StoryLog_Tests.h"
#include "Types/Pokemon_StoryActionArg_Tests.h"
#include "Types/Pokemon_StoryRunTypes_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers every shared AutoStory suite, for the unit test runner.
inline void add_tests_AutoStory(UnitTestDatabase& database){
    add_tests_StoryActionArg(database);
    add_tests_StoryLog(database);
    add_tests_StoryFailureClasses(database);
    add_tests_StoryRunTypes(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
