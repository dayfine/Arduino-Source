/*  PokemonLZA AutoStory - New game intro screens Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryIntroScreens_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryIntroScreens_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the intro screen readers' tests on the console's frames
//  (PokemonLZA/AutoStoryLanguageList/, AutoStoryLookPicker/,
//  AutoStoryNameKeyboard/ and AutoStoryConfirmChoices/ in the test images).
void add_tests_AutoStoryIntroScreens(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
