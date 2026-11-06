/*  BDSP AutoStory Grid Simulation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_GridSim_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_GridSim_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  The grid simulation (Utils/PokemonBDSP_AutoStory_GridSim.h): the moves
//  measured on the console, the story table's routes walked by the route
//  walker on it, and walks that missed doors on the console missing them here.
void add_tests_AutoStory_GridSim(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
