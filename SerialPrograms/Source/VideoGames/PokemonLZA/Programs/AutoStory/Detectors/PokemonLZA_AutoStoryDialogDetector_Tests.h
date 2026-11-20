/*  PokemonLZA AutoStory - Dialog Detector Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDialogDetector_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryDialogDetector_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the tests of StoryDialogDetector: synthetic frames, and the
//  upstream dialog detectors' frames in CommandLineTests.
void add_tests_AutoStoryDialogDetector(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
