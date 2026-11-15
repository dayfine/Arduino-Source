/*  Story Step JSON Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStepJson_Tests_H
#define PokemonAutomation_Pokemon_StoryStepJson_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the lossless shared story-data wire contract.
void add_tests_StoryStepJson(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
