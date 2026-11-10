/*  Screen Story Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Split out of Pokemon_AutoStory_Tests.h, unchanged; it includes this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenStory_Tests_H
#define PokemonAutomation_Pokemon_ScreenStory_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the shared screen events and the judgements over them.
void add_tests_ScreenEvents(UnitTestDatabase& database);
//  The timed screen-event log, the warp judge and the interrupt classifier.
void add_tests_Interrupts(UnitTestDatabase& database);
//  The transition tracker and the games' fade profiles.
void add_tests_Transitions(UnitTestDatabase& database);
//  Story action tables, the variant parser and the table executor.
void add_tests_StoryActions(UnitTestDatabase& database);
//  The table executor's looks for interrupts between steps.
void add_tests_StoryInterrupts(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
