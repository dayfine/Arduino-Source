/*  BDSP AutoStory - Route 204 South tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Route 204's south half as a tile grid, for checkpoint 01.08: from the top
 *  of Jubilife City's north street to the door of the Ravaged Path. The
 *  north half (Route204::north_grid) is reached only through the cave, so
 *  the two halves are separate maps.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Route204South_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Route204South_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route204South{


const Pokemon::TileGrid& grid();

//  Where walking north out of Jubilife's street puts us: its column on the
//  route's bottom rows: the walk stops within two tiles of the border.
std::vector<Pokemon::TilePos> jubilife_entry();

//  The tile below the Ravaged Path's door; walking north from it enters.
extern const Pokemon::TilePos BELOW_DOOR;

//  The trainers whose sight lines a route may cross.
std::vector<Pokemon::TrainerSight> trainer_sights();

//  The sight-line tiles a route can keep out of: all but Sarah's east line,
//  which crosses the whole street.
std::vector<Pokemon::TilePos> tiles_to_avoid();


}
}
}
}
#endif
