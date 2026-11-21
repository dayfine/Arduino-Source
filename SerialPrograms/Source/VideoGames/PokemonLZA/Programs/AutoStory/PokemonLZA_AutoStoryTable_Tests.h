/*  PokemonLZA AutoStory - Story table Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTable_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryTable_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Tests that need no console: the story table lints against PLZA's engine
//  actions, its moves are known, and its autosave rows come first.
void add_tests_AutoStory_StoryTable(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
