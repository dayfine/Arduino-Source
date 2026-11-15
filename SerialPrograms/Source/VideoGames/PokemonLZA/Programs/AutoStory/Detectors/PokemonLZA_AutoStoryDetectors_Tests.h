/*  PokemonLZA AutoStory - Detectors Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDetectors_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryDetectors_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the tests of the AutoStory detectors (Detectors/): synthetic
//  frames, and console frames from CommandLineTests.
void add_tests_AutoStoryDetectors(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
