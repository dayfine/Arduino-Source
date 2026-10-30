/*  BDSP AutoStory - Route 202 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Route 202 (Sandgem Town to Jubilife City) as a tile grid. BDSP keeps the
 *  tile layout of DP, so the grid is read from Bulbapedia's DP map.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Route202_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Route202_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route202{


const Pokemon::TileGrid& grid();

//  Where the player stands in the tall grass after the catching tutorial.
extern const Pokemon::TilePos CATCH_SPOT;
//  Under the south ledge, against the east trees: where the console's 00.04
//  save stands. Two walls pin it.
extern const Pokemon::TilePos SAVE_SPOT;

//  The three trainers, south to north. Which one is Tristan, Natalie or
//  Logan is not known from the map.
extern const Pokemon::TilePos TRAINER_A;
extern const Pokemon::TilePos TRAINER_B;
extern const Pokemon::TilePos TRAINER_C;
extern const Pokemon::TilePos TRAINER_D;

//  Their sight lines, in this map's tiles. The story wants these battles.
const std::vector<Pokemon::TrainerSight>& trainer_sights();

//  A tile in the gap of the north edge; walking north from it leaves for
//  Jubilife City.
extern const Pokemon::TilePos EXIT_JUBILIFE;



}
}
}
}
#endif
