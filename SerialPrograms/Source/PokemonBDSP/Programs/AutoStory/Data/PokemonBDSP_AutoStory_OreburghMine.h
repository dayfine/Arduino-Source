/*  BDSP AutoStory - Oreburgh Mine tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Oreburgh Mine's coal room (Bulbapedia's "B2F") as a tile grid, with
 *  Roark, for checkpoint 01.03.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_OreburghMine_H
#define PokemonAutomation_PokemonBDSP_AutoStory_OreburghMine_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace OreburghMine{


const Pokemon::TileGrid& grid();

//  Where the walk in from the entrance hall leaves us: the top of the rocky
//  corridor, below its timber frame.
const std::vector<Pokemon::TilePos>& corridor_top();

//  Roark, between two rocks a row above the room's bottom row. Talked to from
//  the tile below him, facing north.
extern const Pokemon::TilePos ROARK;
//  The corridor's corner a wall-to-wall route ends on, below a straight
//  walk north out into the entrance hall.
extern const Pokemon::TilePos CORRIDOR_EXIT_START;


}
}
}
}
#endif
