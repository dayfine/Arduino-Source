/*  BDSP AutoStory - Oreburgh City tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Decoded from the Pokemon Diamond decompilation (pret/pokediamond); see
 *  the .cpp's header for the cells, the offset and the objects.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_OreburghCity_H
#define PokemonAutomation_PokemonBDSP_AutoStory_OreburghCity_H

#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace OreburghCity{


const Pokemon::TileGrid& grid();

//  Doors are the tiles walked into. The Center's doorstep: the decomp's door
//  tile is (47, 20), and 20 D-pad taps west from the doorstep stand under
//  the Gym's door (26, 20) on the console (r204 to r215), which puts the
//  doorstep at x 46: one of the two is a tile off, a probe decides.
extern const Pokemon::TilePos POKEMON_CENTER_DOOR;
extern const Pokemon::TilePos POKEMON_CENTER_DOORSTEP;
extern const Pokemon::TilePos GYM_DOOR;
extern const Pokemon::TilePos GYM_DOORSTEP;
//  The Oreburgh Gate's east mouth, walked into heading west from (2, 13).
extern const Pokemon::TilePos GATE_MOUTH;

}
}
}
}
#endif
