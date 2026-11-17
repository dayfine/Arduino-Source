/*  PokemonLZA AutoStory - Map pin reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMapPinReader_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryMapPinReader_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the map pin reader's tests: synthetic frames, and console
//  frames from CommandLineTests/PokemonLZA/AutoStoryDetectors/Zone1Map/.
void add_tests_AutoStoryMapPinReader(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
