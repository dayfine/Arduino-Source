/*  Story Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActions_Tests_H
#define PokemonAutomation_Pokemon_StoryActions_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for story action tables, the variant parser and the table executor.
void add_tests_StoryActions(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
