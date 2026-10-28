/*  BDSP AutoStory - Story maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Route201.h"
#include "PokemonBDSP_AutoStory_Route202.h"
#include "PokemonBDSP_AutoStory_SandgemTown.h"
#include "PokemonBDSP_AutoStory_StoryMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace Pokemon;


const char* const MAP_ROUTE_201 = "Route 201";
const char* const MAP_SANDGEM_TOWN = "Sandgem Town";
const char* const MAP_ROUTE_202 = "Route 202";


//  Offsets come from how the Bulbapedia maps join:
//  -   Route 201 to Sandgem Town: the 16 px strip at the right of the Route
//      201 image matches the left of the Sandgem image, which puts Sandgem's
//      tile column 0 on Route 201's column 64. Rows are 3 px apart, so the
//      row comes from the edges: Sandgem's lab fence (row 6) lines up with
//      the fence post at the right of Route 201's row 3, and Route 201's
//      open rows 4..6 meet Sandgem's open rows 7..9.
//  -   Sandgem Town to Route 202: the 8 px strip at the bottom of the Route
//      202 image matches the top of the Sandgem image 16 px to the right, so
//      Route 202's column x + 1 sits above Sandgem's column x. Route 202's
//      open bottom row (25..30, 25) then meets Sandgem's (24..29, 0), with
//      trees beside both. Sandgem's row 0 is taken as the row below Route
//      202's row 25; the 8 px overlap leaves a half-row doubt.
const MapSet& story_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_ROUTE_201, Route201::grid(), {0, 0});
        maps.add(MAP_SANDGEM_TOWN, SandgemTown::grid(), {64, -3});
        maps.add(MAP_ROUTE_202, Route202::grid(), {63, -29});
        return maps;
    }();
    return maps;
}

bool story_map_plannable(const std::string& map){
    return map == MAP_ROUTE_201 || map == MAP_ROUTE_202;
}

bool story_at_jubilife_exit(const TilePos& world){
    const std::optional<MapTile> tile = story_maps().to_local(world);
    return tile && tile->map == MAP_ROUTE_202 && tile->pos.y <= 3;
}

std::vector<TrainerSight> story_trainer_sights(){
    std::vector<TrainerSight> sights = Route202::trainer_sights();
    for (TrainerSight& sight : sights){
        sight.pos = story_maps().to_world(MAP_ROUTE_202, sight.pos);
    }
    return sights;
}



}
}
}
