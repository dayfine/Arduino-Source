/*  BDSP AutoStory - Route 203 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Route 203 as a tile grid, for checkpoint 01.06: from the Oreburgh Gate's
 *  west exit to Jubilife City's east edge.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Route203_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Route203_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route203{


const Pokemon::TileGrid& grid();

//  Where stepping out of the Oreburgh Gate may leave us when Route 203's
//  banner shows: the tiles west of the cave's mouth.
const std::vector<Pokemon::TilePos>& gate_exit();


}
}
}
}
#endif
