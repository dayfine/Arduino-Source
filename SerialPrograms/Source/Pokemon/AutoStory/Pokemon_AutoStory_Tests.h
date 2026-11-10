/*  AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the shared story engine (Pokemon/AutoStory).
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStory_Tests_H
#define PokemonAutomation_Pokemon_AutoStory_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for story event sequencing and execution.
void add_tests_StoryEvents(UnitTestDatabase& database);
// Registers unit tests for story segment running, verification, and branching.
void add_tests_StorySegments(UnitTestDatabase& database);
// Registers unit tests for high-level story step dispatching and table conversion.
void add_tests_StorySteps(UnitTestDatabase& database);



//  Lossless shared story-data wire contract.
void add_tests_StoryStepJson(UnitTestDatabase& database);

//  Shared authored envelope, game identity and embedded-resource loading.
void add_tests_AuthoredCheckpointJson(UnitTestDatabase& database);

} // namespace Pokemon
} // namespace PokemonAutomation
#endif
