/*  BDSP AutoStory Map Grid Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_MapGrid_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_MapGrid_Tests_H

#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  Offline checks of the decomp grids against what the console showed:
//  Route 202's catch patch between tree rows and the walks in and out of
//  it, and Sandgem's Pokemon Center door, its sign and the row below them.
void add_tests_AutoStory_MapGrids(UnitTestDatabase& database);



}
}
}
#endif
