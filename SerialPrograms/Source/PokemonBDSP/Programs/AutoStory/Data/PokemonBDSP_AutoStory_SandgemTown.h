/*  BDSP AutoStory - Sandgem Town tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Sandgem Town as a tile grid, read from Bulbapedia's DP map; BDSP keeps
 *  DP's tile layout.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_SandgemTown_H
#define PokemonAutomation_PokemonBDSP_AutoStory_SandgemTown_H

#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace SandgemTown{


const Pokemon::TileGrid& grid();

extern const Pokemon::TilePos LAB_DOOR;
extern const Pokemon::TilePos POKEMON_CENTER_DOOR;
extern const Pokemon::TilePos POKE_MART_DOOR;



}
}
}
}
#endif
