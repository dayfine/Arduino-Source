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


//  Registers read_dialog_choice_rows()'s tests: synthetic menus of two and
//  three rows always; console B's choice menus from the test images.
void add_tests_AutoStoryDialogChoiceBox(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
