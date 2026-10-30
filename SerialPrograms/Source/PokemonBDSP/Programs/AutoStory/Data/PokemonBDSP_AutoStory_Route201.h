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

//  The Poke Mart lady's row, (42..63, 3): she wanders its tall-grass part,
//  (42..52, 3), and is not on the grid. The row's west end is a corner with
//  trees west and north of it.
extern const Pokemon::TilePos POTION_GIVER_ROW_WEST_END;
//  The open tile east of the grass to walk west from, to meet her.
extern const Pokemon::TilePos POTION_GIVER_APPROACH;
//  The row's east end, against Sandgem Town's fence.
extern const Pokemon::TilePos POTION_GIVER_ROW_EAST_END;



}
}
}
}
#endif
