/*  AutoStory Sim Golden Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySimGolden_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStorySimGolden_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What the overworld sim does with each row of the story, clean and under
//  the fault sweep, against the goldens beside the tests
//  (PokemonFRLG_AutoStorySimGolden_Tests.cpp).
void add_tests_AutoStorySimGolden(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
