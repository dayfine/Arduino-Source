/*  Checkpoint Retry Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_CheckpointRetry_Tests_H
#define PokemonAutomation_Pokemon_CheckpointRetry_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the checkpoint retry loop and checkpoint saves.
void add_tests_CheckpointRetry(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
