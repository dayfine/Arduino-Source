/*  PokemonLZA AutoStory - Zone gate reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZoneGateReader_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStoryZoneGateReader_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Registers the zone gate reader's tests: synthetic frames, and console
//  frames from CommandLineTests/PokemonLZA/AutoStoryDetectors/Zone1Gate/.
void add_tests_AutoStoryZoneGateReader(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
