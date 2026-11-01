/*  BDSP AutoStory - Floaroma Town tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Decoded from the Pokemon Diamond decompilation (pret/pokediamond); see
 *  the .cpp's header for the cells, the offset and the objects.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_FloaromaTown_H
#define PokemonAutomation_PokemonBDSP_AutoStory_FloaromaTown_H

#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace FloaromaTown{


const Pokemon::TileGrid& grid();

//  Where 01.10's walk north from Route 204 ends on the town's banner and
//  saves (r248a, r251a): the path's middle column, three rows in.
extern const Pokemon::TilePos ROUTE_204_ARRIVAL;
//  The old couple in the flower field, placed from the console (2026-10-31
//  hand walk and r251a: eight tiles west of the arrival, then three north
//  to the tile below her); not in the DP data.
extern const Pokemon::TilePos OLD_WOMAN;
extern const Pokemon::TilePos OLD_MAN;
extern const Pokemon::TilePos POKEMON_CENTER_DOOR;

}
}
}
}
#endif
