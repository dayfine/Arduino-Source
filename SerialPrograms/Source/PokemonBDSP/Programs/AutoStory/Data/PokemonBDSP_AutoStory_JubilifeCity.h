/*  BDSP AutoStory - Jubilife City tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Jubilife City as a tile grid, from the Diamond decompilation's collision
 *  table, with the places and people of checkpoint 00.05.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_JubilifeCity_H
#define PokemonAutomation_PokemonBDSP_AutoStory_JubilifeCity_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace JubilifeCity{


const Pokemon::TileGrid& grid();

//  Where we may stand after climbing the steps from where 00.05 starts:
//  below them, against the wall just east of them, then a tile west and
//  about two tiles north.
const std::vector<Pokemon::TilePos>& up_the_steps();

//  Doors: walk north from the tile below to enter.
extern const Pokemon::TilePos TRAINERS_SCHOOL_DOOR;
extern const Pokemon::TilePos POKEMON_CENTER_DOOR;
extern const Pokemon::TilePos TV_STATION_DOOR;

//  The Poketch Company president and the three coupon clowns.
extern const Pokemon::TilePos POKETCH_PRESIDENT;
extern const Pokemon::TilePos CLOWN_MART;
extern const Pokemon::TilePos CLOWN_TV_STATION;
extern const Pokemon::TilePos CLOWN_POKETCH_COMPANY;



}
}
}
}
#endif
