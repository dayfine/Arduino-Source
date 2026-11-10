/*  Story Run Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the pieces every AutoStory program shares, story
 *  engine or not: the checkpoint log lines and the range options.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRun_Tests_H
#define PokemonAutomation_Pokemon_StoryRun_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the shared checkpoint log lines and stats counts.
void add_tests_StoryLog(UnitTestDatabase& database);
// Registers unit tests for the shared start/end checkpoint range options.
void add_tests_StoryRangeOptions(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
