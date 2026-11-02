/*  BDSP AutoStory - Story maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The maps the story walks through, joined into story worlds, and the
 *  places and trainers on them that the story's walks need. The tiles and
 *  named points come from the JSON maps (PokemonBDSP_AutoStory_Maps.h);
 *  where the maps sit relative to each other, and the trainers' sight
 *  ranges, the maps do not say, so they are here.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryMaps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryMaps_H

#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Names of the maps in the story worlds below. Each is placed from the
//  JSON map named without spaces ("Route 201" from Route201.json), except
//  MAP_ROUTE_204, which is Route204North.json.
extern const char* const MAP_ROUTE_201;
extern const char* const MAP_SANDGEM_TOWN;
extern const char* const MAP_ROUTE_202;
extern const char* const MAP_JUBILIFE_CITY;
extern const char* const MAP_OREBURGH_MINE;
extern const char* const MAP_RAVAGED_PATH;
extern const char* const MAP_ROUTE_204;
extern const char* const MAP_ROUTE_204_SOUTH;
extern const char* const MAP_ROUTE_203;
extern const char* const MAP_OREBURGH_CITY;
extern const char* const MAP_FLOAROMA_TOWN;

//  Route 201, Sandgem Town and Route 202.
const StoryWorld& story_maps();

//  Jubilife City on its own: 00.05 starts there after Dawn's scene, so it is
//  not joined to Route 202.
const StoryWorld& jubilife_maps();
//  Route 203 on its own (01.06), and whether a tile is on row 18 between the
//  two cliffs: row 18 runs straight west from there under the west cliff
//  into Jubilife, so the plan ends there (no leg may leave the map) and a
//  walk west until Jubilife's banner shows goes on from there.
const StoryWorld& route203_maps();
bool on_route203_west_line(const Pokemon::TilePos& pos);
//  Jubilife City's north-east corner (01.06), at its own tile coordinates:
//  the rest of the city is walled off, so the plan searches only the part
//  the walk needs. Where Route 203's road enters it, and whether a tile is
//  on the north road below the plaza: walking north from there goes up the
//  road to Route 204 (and Rowan's scene).
const StoryWorld& jubilife_east_maps();
const std::vector<Pokemon::TilePos>& jubilife_east_entrance();
bool on_jubilife_north_road(const Pokemon::TilePos& pos);
//  The Oreburgh Mine's coal room on its own (01.03).
const StoryWorld& oreburgh_mine_maps();

//  The Ravaged Path on its own (01.05), with the rocks in `smashed` open.
StoryWorld ravaged_path_maps(const std::vector<Pokemon::TilePos>& smashed = {});

//  Route 204's north half on its own (01.05), and whether a tile is on the
//  path north into Floaroma.
const StoryWorld& route204_north_maps();
bool in_floaroma_path(const Pokemon::TilePos& pos);

//  Route 204's south half on its own (01.08): it joins the north half only
//  through the Ravaged Path, so the two are separate maps.
const StoryWorld& route204_south_maps();

//  Oreburgh City alone (01.04, 01.05), and Floaroma Town alone (01.11).
const StoryWorld& oreburgh_city_maps();
const StoryWorld& floaroma_maps();

//  Whether a plan may cross this map. Towns are crossed by screen-driven
//  legs until the console has confirmed their maps.
bool story_map_plannable(const std::string& map);

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
std::vector<Pokemon::TilePos> story_door_front(const char* map, const Pokemon::TilePos& door);

//  Where we may stand after training in the catch grass, in world tiles: the
//  training walks up and down the catch spot's column, between the tree walls
//  north and south of the grass, so a battle may stop us on any of its
//  three rows. A column either side allows for where the catch left us.
std::vector<Pokemon::TilePos> story_after_training_area();

//  Route 202's trainers, in world tiles of story_maps().
std::vector<TrainerSight> story_trainer_sights();

//  The trainers of a map, in its own tiles, with the sight ranges that fit
//  the console. A trainer who looks two ways is listed once per way.
std::vector<TrainerSight> route202_trainer_sights();
std::vector<TrainerSight> route204_north_trainer_sights();
std::vector<TrainerSight> route204_south_trainer_sights();
std::vector<TrainerSight> oreburgh_gym_trainer_sights();

//  Route 204 South's tiles in the trainers' sight that a walk can avoid:
//  all but Sarah's east line, which crosses the street (see the sights).
std::vector<Pokemon::TilePos> route204_south_tiles_to_avoid();



}
}
}
#endif
