/*  BDSP AutoStory - Story maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_JubilifeCity.h"
#include "PokemonBDSP_AutoStory_OreburghMine.h"
#include "PokemonBDSP_AutoStory_RavagedPath.h"
#include "PokemonBDSP_AutoStory_Route203.h"
#include "PokemonBDSP_AutoStory_Route204.h"
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
const char* const MAP_OREBURGH_MINE = "Oreburgh Mine";
const char* const MAP_RAVAGED_PATH = "Ravaged Path";
const char* const MAP_ROUTE_204 = "Route 204";
const char* const MAP_ROUTE_203 = "Route 203";


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

//  Planned one map at a time: Route 203 and Jubilife joined ran for minutes
//  without a plan (r224a, 2026-10-31).
const MapSet& route203_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_ROUTE_203, Route203::grid(), {0, 0});
        return maps;
    }();
    return maps;
}

bool on_route203_west_line(const TilePos& pos){
    return pos.y == 17 && pos.x >= 38 && pos.x <= 42;
}

//  Columns 40..63, rows 8..20 of the Jubilife grid, as a map of its own:
//  the cut is the map's edge, which no leg may
//  cross or stop on, not a wall a plan could walk "to". In this corner the
//  white curbs either side of the north road (columns 42 and 47) are opened:
//  the console showed the main road's curbs are not walls (2026-10-30), and
//  these look the same; not walked yet.
namespace{
const TilePos JUBILIFE_EAST_CORNER{40, 8};
}
const MapSet& jubilife_east_maps(){
    static const MapSet maps = []{
        const TileGrid& city = JubilifeCity::grid();
        const TilePos corner = JUBILIFE_EAST_CORNER;
        std::vector<std::string> rows;
        for (int y = corner.y; y <= 20; y++){
            std::string row;
            for (int x = corner.x; x < city.width(); x++){
                const TileType type = city.at({x, y});
                const bool curb = (x == 42 || x == 47) && y <= 16 && type == TileType::BLOCKED;
                row += tile_char(curb ? TileType::WALKABLE : type);
            }
            rows.push_back(row);
        }
        MapSet maps;
        maps.add(MAP_JUBILIFE_CITY, TileGrid(rows), {0, 0});
        return maps;
    }();
    return maps;
}

//  Route 203's road (rows 17..19) meets Jubilife's east stairs, rows 16..17
//  of its grid, at its east edge; the banner shows a tile or two in. Both
//  in the corner map's tiles, from the Jubilife grid's.
const std::vector<TilePos>& jubilife_east_entrance(){
    static const std::vector<TilePos> tiles = []{
        std::vector<TilePos> tiles;
        for (int y = 16; y <= 17; y++){
            for (int x = 61; x <= 63; x++){
                tiles.push_back({x - JUBILIFE_EAST_CORNER.x, y - JUBILIFE_EAST_CORNER.y});
            }
        }
        return tiles;
    }();
    return tiles;
}

bool on_jubilife_north_road(const TilePos& pos){
    const int x = pos.x + JUBILIFE_EAST_CORNER.x;
    const int y = pos.y + JUBILIFE_EAST_CORNER.y;
    return x >= 43 && x <= 46 && y >= 12 && y <= 14;
}

const MapSet& oreburgh_mine_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_OREBURGH_MINE, OreburghMine::grid(), {0, 0});
        return maps;
    }();
    return maps;
}

MapSet ravaged_path_maps(const std::vector<TilePos>& smashed){
    TileGrid grid = RavagedPath::grid();
    for (const TilePos& rock : smashed){
        grid.set(rock, TileType::WALKABLE);
    }
    MapSet maps;
    maps.add(MAP_RAVAGED_PATH, grid, {0, 0});
    return maps;
}

const MapSet& route204_north_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_ROUTE_204, Route204::north_grid(), {0, 0});
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

std::vector<TilePos> story_save_spots(){
    const MapSet& maps = story_maps();
    return {
        maps.to_world(MAP_ROUTE_202, Route202::CATCH_SPOT),
        maps.to_world(MAP_ROUTE_202, Route202::SAVE_SPOT),
    };
}

std::vector<TilePos> story_door_front(const char* map, const TilePos& door){
    const TilePos below = story_maps().to_world(map, door.next(Direction::SOUTH));
    return {below.next(Direction::WEST), below, below.next(Direction::EAST)};
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
