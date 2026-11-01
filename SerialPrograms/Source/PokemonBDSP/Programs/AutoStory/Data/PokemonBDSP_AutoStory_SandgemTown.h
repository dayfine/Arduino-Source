/*  BDSP AutoStory - Sandgem Town tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Sandgem Town as a tile grid, from the Diamond decompilation's collision
 *  table; BDSP keeps DP's tile layout.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_SandgemTown_H
#define PokemonAutomation_PokemonBDSP_AutoStory_SandgemTown_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace SandgemTown{


const Pokemon::TileGrid& grid();

//  Where the walk south from Route 202 down the road's east side stops,
//  above the Poke Mart where the road narrows.
extern const Pokemon::TilePos ROUTE_202_ENTRY_STOP;
extern const Pokemon::TilePos LAB_DOOR;
extern const Pokemon::TilePos POKEMON_CENTER_DOOR;
//  The sign east of the Pokemon Center's door. Walking north from row 14 in
//  its column stops on the tile below it.
extern const Pokemon::TilePos POKEMON_CENTER_SIGN;
extern const Pokemon::TilePos POKE_MART_DOOR;
//  The east end of the row below the Center's and the Mart's doors, at the
//  trees: one known tile to plan the way north to Route 202 from. From the
//  Mart's door, a tile either side of its column, no plan north is robust.
extern const Pokemon::TilePos FRONT_ROW_EAST_END;
//  The doors' front row below the gap between the Center's sign and the
//  Mart's, columns 21..24: the only way north from that row that passes no
//  door. Walking north from any of them leaves town for Route 202. Walking
//  north from anywhere else on the row between columns 18 and 26 stops at a
//  sign or a wall; the Center's door is at 17, the Mart's at 27.
std::vector<Pokemon::TilePos> below_sign_gap();



}
}
}
}
#endif
