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
//  frames always; the opening's frames from the folders in
//  PLZA_OPENING_FRAMES and PLZA_USER_OPENING_FRAMES, and the runs' frames
//  from PLZA_0101_RUN1_FRAMES and the others, each skipped when unset.
void add_tests_AutoStoryDetectors(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
