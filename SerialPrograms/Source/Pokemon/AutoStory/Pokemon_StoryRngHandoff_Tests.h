/*  Story RNG Handoff Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRngHandoff_Tests_H
#define PokemonAutomation_Pokemon_StoryRngHandoff_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the shared RNG handoff contract.
void add_tests_StoryRngHandoff(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
