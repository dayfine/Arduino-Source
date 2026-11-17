/*  PokemonLZA AutoStory - Dialog choice box Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDialogChoiceBox_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryDialogChoiceBox_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the tests of the choice rows reader on synthetic menus and on
//  console frames from CommandLineTests, and of the screen events' choice
//  box on a console frame.
void add_tests_AutoStoryDialogChoiceBox(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
