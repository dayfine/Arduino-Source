/*  Story Step JSON Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStepJson_Tests_H
#define PokemonAutomation_Pokemon_StoryStepJson_Tests_H

#include <cstddef>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the lossless shared story-data wire contract.
void add_tests_StoryStepJson(UnitTestDatabase& database);

#ifdef PA_STORY_DATA_STANDALONE
//  Standalone StoryDataTests only: runs the built-in cases, prints one line
//  per case, returns how many failed.
size_t run_story_step_json_cases();
#endif



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
