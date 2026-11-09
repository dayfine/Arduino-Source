/*  BDSP AutoStory Grid Simulation - Wild Battles on Wall Legs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WallBattleSim_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WallBattleSim_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  A wild battle on a leg to a wall, on the grid simulation: run from, or
//  fought when the game refuses the escape, the leg still ends at the wall.
void add_tests_AutoStory_WallBattleSim(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
