/*  BDSP AutoStory - Story spots
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Spots on the Route 201 to Route 202 world (story_maps()) where
 *  segment 00's walks start, save and end, in world tiles.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StorySpots_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StorySpots_H

#include <string_view>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The open rows at the top of Route 202, in world tiles: walking north from
//  any of them enters Jubilife City.
bool story_at_jubilife_exit(const Pokemon::TilePos& world);

//  Where the post-catch half of 00.04 may start, in world tiles: the catch
//  spot (00.03 saves there after the catch) and the save spot under the
//  ledge (00.04 saves there once it has Potions, before Route 202's trainers).
std::vector<Pokemon::TilePos> story_save_spots();

//  Where 00.04b's save after the catch may stand, in world tiles: wherever
//  the catch's walks in the grass ended, the patch (15..20, 25..27) and the
//  lawn column east of it, (21, 25..29), down to the pocket behind the tree
//  row where r252's catch ended and saved (the save loads there: (21, 29)
//  by the screen, the 202 sign 3 columns right and 4 rows up).
std::vector<Pokemon::TilePos> story_catch_save_area();

//  Where a walk out of the door `door` of `map` may leave us, in world tiles:
//  the tile below it and its neighbours either side, since the door opening
//  is not lined up with one tile (Jubilife's Center, r85).
std::vector<Pokemon::TilePos> story_door_front(std::string_view map, const Pokemon::TilePos& door);

//  Where we may stand after training in the catch grass, in world tiles: the
//  training walks up and down the catch spot's column, between the tree walls
//  north and south of the grass, so a battle may stop us on any of its
//  three rows. A column either side allows for where the catch left us.
std::vector<Pokemon::TilePos> story_after_training_area();


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
