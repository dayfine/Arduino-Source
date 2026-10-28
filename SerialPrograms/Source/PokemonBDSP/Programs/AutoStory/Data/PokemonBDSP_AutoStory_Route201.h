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

//  The girl who gives Potions, in the top row of the north-east tall grass,
//  (42..52, 3), one tile from its west end.
extern const Pokemon::TilePos POTION_GIVER;
//  West of her, against the trees on two sides: talk to her from here, facing east.
extern const Pokemon::TilePos POTION_GIVER_TALK_FROM;
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
