/*  Overworld Gate Maps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldGateMaps_Tests_H
#define PokemonAutomation_PokemonFRLG_OverworldGateMaps_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Registers the gate tests on the real Silph Co. and Mansion maps: the map reader's gates, switches and one-way
//  warps, the gate state, and the gated planner.
void add_tests_OverworldGateMaps(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
