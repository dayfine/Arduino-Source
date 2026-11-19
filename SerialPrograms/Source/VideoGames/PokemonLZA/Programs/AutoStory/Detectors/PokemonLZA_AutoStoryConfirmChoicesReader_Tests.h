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


//  Registers the Confirm Choices reader's tests; those on PLZA_INTRO_SCREENS are
//  skipped when it is unset.
void add_tests_AutoStoryConfirmChoicesReader(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
