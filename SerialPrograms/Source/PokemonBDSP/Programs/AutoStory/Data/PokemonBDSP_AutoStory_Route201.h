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

//  The sign by the Sandgem Town border, at the east end of the route.
extern const Pokemon::TilePos BORDER_SIGN;
//  The Poke Mart lady, who gives 10 Potions: at the west end of the top
//  grass row. Not on the grid: she turns and may wander.
extern const Pokemon::TilePos POTION_GIVER;
//  East of her: walking west along her row from the open ground east of the
//  grass stops here, facing her.
extern const Pokemon::TilePos POTION_GIVER_TALK_FROM;



}
}
}
}
#endif
