/*  BDSP AutoStory - Story maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_JubilifeCity.h"
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
const char* const MAP_JUBILIFE_CITY = "Jubilife City";


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

const MapSet& jubilife_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_JUBILIFE_CITY, JubilifeCity::grid(), {0, 0});
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

std::vector<TilePos> story_potion_giver_area(){
    const MapSet& maps = story_maps();
    std::vector<TilePos> area;
    const TilePos west = maps.to_world(MAP_ROUTE_201, Route201::POTION_GIVER_ROW_WEST_END);
    const TilePos east = maps.to_world(MAP_ROUTE_201, Route201::POTION_GIVER_ROW_EAST);
    for (int y = west.y; y <= west.y + 1; y++){
        for (int x = west.x; x <= east.x; x++){
            if (maps.world().standable({x, y})){
                area.push_back({x, y});
            }
        }
    }
    return area;
}

std::vector<TilePos> story_after_training_area(){
    const TilePos catch_spot = story_maps().to_world(MAP_ROUTE_202, Route202::CATCH_SPOT);
    std::vector<TilePos> area;
    for (int y = catch_spot.y - 2; y <= catch_spot.y; y++){
        for (int x = catch_spot.x - 1; x <= catch_spot.x + 1; x++){
            area.push_back({x, y});
        }
    }
    return area;
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
