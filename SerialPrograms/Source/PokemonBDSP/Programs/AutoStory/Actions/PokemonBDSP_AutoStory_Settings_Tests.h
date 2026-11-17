/*  BDSP AutoStory - Settings Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Settings_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Settings_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  The Options rows the settings step walks down per game language, and
//  the selected value's column from where its red ink is.
void add_tests_AutoStory_Settings(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
