/*  PokemonLZA AutoStory - Confirm Choices reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConfirmChoicesReader_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryConfirmChoicesReader_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the Confirm Choices reader's tests: synthetic frames always;
//  console B's frames in each language from the test images
//  (PokemonLZA/AutoStoryConfirmChoices/), and the other intro screens from
//  the folder in PLZA_INTRO_SCREENS, skipped when unset.
void add_tests_AutoStoryConfirmChoicesReader(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
