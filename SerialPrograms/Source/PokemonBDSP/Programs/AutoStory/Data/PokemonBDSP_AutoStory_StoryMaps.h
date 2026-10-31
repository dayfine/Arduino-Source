/*  BDSP AutoStory - Story maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The maps the story walks through, joined on one grid.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryMaps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryMaps_H

#include "Pokemon/Navigation/Pokemon_MapSet.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


extern const char* const MAP_ROUTE_201;
extern const char* const MAP_SANDGEM_TOWN;
extern const char* const MAP_ROUTE_202;
extern const char* const MAP_JUBILIFE_CITY;
extern const char* const MAP_OREBURGH_MINE;
extern const char* const MAP_RAVAGED_PATH;
extern const char* const MAP_ROUTE_204;
extern const char* const MAP_ROUTE_203;

//  Route 201, Sandgem Town and Route 202.
const Pokemon::MapSet& story_maps();

//  Jubilife City on its own: 00.05 starts there after Dawn's scene, so it is
//  not joined to Route 202.
const Pokemon::MapSet& jubilife_maps();
//  Route 203 on its own (01.06), and whether a tile is on its road a few
//  tiles from Jubilife: the plan ends there, since no leg may leave the map,
//  and a walk west until Jubilife's banner shows goes on from there.
const Pokemon::MapSet& route203_maps();
bool near_route203_west_end(const Pokemon::TilePos& pos);
//  Jubilife City's north-east corner (01.06), at Jubilife's own tile
//  coordinates: the rest of the city is walled off, so the plan searches only
//  the part the walk needs. Where Route 203's road enters it, and whether a
//  tile is on the north road below the plaza: walking north from there goes
//  up the road to Route 204 (and Rowan's scene).
const Pokemon::MapSet& jubilife_east_maps();
const std::vector<Pokemon::TilePos>& jubilife_east_entrance();
bool on_jubilife_north_road(const Pokemon::TilePos& pos);
//  The Oreburgh Mine's coal room on its own (01.03).
const Pokemon::MapSet& oreburgh_mine_maps();

//  The Ravaged Path on its own (01.05), with the rocks in `smashed` open.
Pokemon::MapSet ravaged_path_maps(const std::vector<Pokemon::TilePos>& smashed = {});

//  Route 204's north half on its own (01.05).
const Pokemon::MapSet& route204_north_maps();

//  Whether a plan may cross this map. Routes are traced from the DP maps and
//  their tree walls carried over to BDSP; towns changed more in the remake, so
//  a town is crossed by screen-driven legs until the console has confirmed its
//  grid.
bool story_map_plannable(const std::string& map);

//  The open rows at the top of Route 202, in world tiles: walking north from
//  any of them enters Jubilife City.
bool story_at_jubilife_exit(const Pokemon::TilePos& world);

//  Where the post-catch half of 00.04 may start, in world tiles: the catch
//  spot (00.03 saves there after the catch) and the save spot under the
//  ledge (00.04 saves there once it has Potions, before Route 202's trainers).
std::vector<Pokemon::TilePos> story_save_spots();

//  Where a walk out of the door `door` of `map` may leave us, in world tiles:
//  the tile below it and its neighbours either side, since the door opening
//  is not lined up with one tile (Jubilife's Center, r85).
std::vector<Pokemon::TilePos> story_door_front(const char* map, const Pokemon::TilePos& door);

//  Where we may stand after training in the catch grass, in world tiles: the
//  training walks up and down the catch spot's column, between the tree walls
//  north and south of the grass, so a battle may stop us on any of its three
//  rows. A column either side allows for where the catch left us.
std::vector<Pokemon::TilePos> story_after_training_area();

//  Route 202's trainers, in world tiles of story_maps().
std::vector<Pokemon::TrainerSight> story_trainer_sights();



}
}
}
#endif
