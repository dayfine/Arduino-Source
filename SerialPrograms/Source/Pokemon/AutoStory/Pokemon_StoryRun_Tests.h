/*  Story Run Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the pieces every AutoStory program shares, story
 *  engine or not: the checkpoint log lines and the stats counts.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRun_Tests_H
#define PokemonAutomation_Pokemon_StoryRun_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the shared checkpoint log lines.
void add_tests_StoryLog(UnitTestDatabase& database);
// Registers unit tests for the story counts added to the AutoStory stats.
void add_tests_AutoStoryStats(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
