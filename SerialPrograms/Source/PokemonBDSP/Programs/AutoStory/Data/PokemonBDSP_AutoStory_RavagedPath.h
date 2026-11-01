/*  BDSP AutoStory - Ravaged Path tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Ravaged Path (the cave on Route 204) as a tile grid, for checkpoint
 *  01.05: from its south door, where Route 204's south half enters, east
 *  along its lower corridor to the door out onto Route 204's north half.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RavagedPath_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RavagedPath_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace RavagedPath{


//  With every Rock Smash rock in place (blocked).
const Pokemon::TileGrid& grid();

//  The Rock Smash rocks of the traced part, blocked in grid(). Each breaks
//  for good once smashed, so a plan after a smash opens that tile.
const std::vector<Pokemon::TilePos>& rocks();

//  Where walking in from Route 204's south half leaves us: the tile above the
//  south door, at the foot of the room below the stairs.
extern const Pokemon::TilePos ENTRANCE;
//  The tiles west of the two rocks across the lower corridor: from either,
//  facing east, A offers Rock Smash, and the corridor is open to its east end.
const std::vector<Pokemon::TilePos>& before_rocks();
//  The corridor's east end on its bottom row, where walls stop a walk east
//  and south: a route can end here, and nowhere else near the way out.
extern const Pokemon::TilePos EAST_END;
//  The tile above the door out onto Route 204's north half, a tile west of
//  EAST_END: walk south.
extern const Pokemon::TilePos ABOVE_EXIT;


}
}
}
}
#endif
