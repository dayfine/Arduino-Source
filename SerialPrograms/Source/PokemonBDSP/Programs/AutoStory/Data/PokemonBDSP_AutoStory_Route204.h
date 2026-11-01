/*  BDSP AutoStory - Route 204 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Route 204's north half as a tile grid, for checkpoint 01.05: from the
 *  door out of the Ravaged Path up to the path into Floaroma Town.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Route204_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Route204_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route204{


const Pokemon::TileGrid& north_grid();

//  Where leaving the Ravaged Path puts us: the tile below its door.
extern const Pokemon::TilePos CAVE_EXIT;

//  Whether a tile is in the path north into Floaroma, between the flower
//  beds' fences: walking north from here leaves the route.
bool in_floaroma_path(const Pokemon::TilePos& pos);

//  The twins below the right-hand flower bed: a double battle when they see
//  us walk past below them.
std::vector<Pokemon::TrainerSight> north_trainer_sights();

//  The corners of the line into Floaroma from the cave's exit, as a human
//  walked it (2026-10-31): the last is in the path into Floaroma.
std::vector<Pokemon::TilePos> floaroma_waypoints();


}
}
}
}
#endif
