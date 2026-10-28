/*  BDSP AutoStory - Route 202 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Route 202 (Sandgem Town to Jubilife City) as a tile grid. BDSP keeps the
 *  tile layout of DP, so the grid is read from Bulbapedia's DP map.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Route202_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Route202_H

#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route202{


const Pokemon::TileGrid& grid();

//  Where the player stands in the tall grass after the catching tutorial.
extern const Pokemon::TilePos CATCH_SPOT;

//  The girl who gives a Potion, and the tile to talk to her from (face north).
extern const Pokemon::TilePos POTION_GIVER;
extern const Pokemon::TilePos POTION_GIVER_FRONT;

//  Trainers. Names follow Bulbapedia's list order (south to north); the map
//  shows sprites, not names.
extern const Pokemon::TilePos TRAINER_TRISTAN;
extern const Pokemon::TilePos TRAINER_NATALIE;
extern const Pokemon::TilePos TRAINER_LOGAN;

//  A tile in the gap of the north edge; walking north from it leaves for
//  Jubilife City.
extern const Pokemon::TilePos EXIT_JUBILIFE;



}
}
}
}
#endif
