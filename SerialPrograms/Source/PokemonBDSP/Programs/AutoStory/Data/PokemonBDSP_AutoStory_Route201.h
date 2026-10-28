/*  BDSP AutoStory - Route 201 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Route 201 (Twinleaf Town to Sandgem Town) as a tile grid, read from
 *  Bulbapedia's DP map; BDSP keeps DP's tile layout.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Route201_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Route201_H

#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route201{


const Pokemon::TileGrid& grid();

//  The girl who gives a Potion, in the top row of the north-east tall grass,
//  (42..52, 3).
extern const Pokemon::TilePos POTION_GIVER;
//  The tile east of her row, against the trees: walk west from here to meet her.
extern const Pokemon::TilePos POTION_GIVER_ROW_EAST;
//  The first and last tiles of her row.
extern const Pokemon::TilePos POTION_GIVER_ROW_WEST_END;
extern const Pokemon::TilePos POTION_GIVER_ROW_EAST_END;



}
}
}
}
#endif
