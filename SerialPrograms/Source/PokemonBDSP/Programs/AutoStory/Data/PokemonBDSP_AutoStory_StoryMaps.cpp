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
#include "PokemonBDSP_AutoStory_Route204South.h"
#include "PokemonBDSP_AutoStory_OreburghCity.h"
#include "PokemonBDSP_AutoStory_FloaromaTown.h"
#include "PokemonBDSP_AutoStory_Route201.h"
#include "PokemonBDSP_AutoStory_Route202.h"
#include "PokemonBDSP_AutoStory_SandgemTown.h"
#include "PokemonBDSP_AutoStory_MapFiles.h"
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
const char* const MAP_OREBURGH_CITY = "Oreburgh City";
const char* const MAP_FLOAROMA_TOWN = "Floaroma Town";
const char* const MAP_ROUTE_204_SOUTH = "Route 204 South";
const char* const MAP_ROUTE_203 = "Route 203";


//  The grids are story_grid()'s: the JSON map files once the program has
//  loaded them, the C++ tables in the offline tests.
//
//  Offsets are the maps' places in the game's world grid (the decomp's
//  cells): Route 201 at world (96,839), Sandgem Town at (160,832), Route 202
//  at (159,800). Route 201's row 4 is Sandgem's row 11, below the doors, and
//  Route 202's road comes down off its row 31 onto Sandgem's row 0.
const MapSet& story_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_ROUTE_201, story_grid("Route201"), {0, 0});
        maps.add(MAP_SANDGEM_TOWN, story_grid("SandgemTown"), {64, -7});
        maps.add(MAP_ROUTE_202, story_grid("Route202"), {63, -39});
        return maps;
    }();
    return maps;
}

const MapSet& jubilife_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_JUBILIFE_CITY, story_grid("JubilifeCity"), {0, 0});
        return maps;
    }();
    return maps;
}

//  Planned one map at a time: Route 203 and Jubilife joined ran for minutes
//  without a plan (r224a, 2026-10-31).
const MapSet& route203_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_ROUTE_203, story_grid("Route203"), {0, 0});
        return maps;
    }();
    return maps;
}

bool on_route203_west_line(const TilePos& pos){
    return pos.y == 18 && pos.x >= 24 && pos.x <= 42;
}

//  Columns 42..63, rows 12..25 of the Jubilife grid (world x 170..191,
//  z 748..761), as a map of its own: the cut is the map's edge, which no leg
//  may cross or stop on, not a wall a plan could walk "to".
namespace{
const TilePos JUBILIFE_EAST_CORNER{42, 12};
const int JUBILIFE_EAST_BOTTOM = 25;
}
const MapSet& jubilife_east_maps(){
    static const MapSet maps = []{
        const TileGrid city = story_grid("JubilifeCity");
        const TilePos corner = JUBILIFE_EAST_CORNER;
        std::vector<std::string> rows;
        for (int y = corner.y; y <= JUBILIFE_EAST_BOTTOM; y++){
            std::string row;
            for (int x = corner.x; x < city.width(); x++){
                row += tile_char(city.at({x, y}));
            }
            rows.push_back(row);
        }
        MapSet maps;
        maps.add(MAP_JUBILIFE_CITY, TileGrid(rows), {0, 0});
        return maps;
    }();
    return maps;
}

//  Route 203's road (rows 17..19 of its grid) meets Jubilife's east street,
//  rows 21..24 of its grid, at its east edge; the banner shows a tile or two
//  in. Both in the corner map's tiles, from the Jubilife grid's.
const std::vector<TilePos>& jubilife_east_entrance(){
    static const std::vector<TilePos> tiles = []{
        std::vector<TilePos> tiles;
        for (int y = 21; y <= 24; y++){
            for (int x = 61; x <= 63; x++){
                tiles.push_back({x - JUBILIFE_EAST_CORNER.x, y - JUBILIFE_EAST_CORNER.y});
            }
        }
        return tiles;
    }();
    return tiles;
}

//  The north road where it leaves the plaza, columns 44..48 of rows 16..20.
bool on_jubilife_north_road(const TilePos& pos){
    const int x = pos.x + JUBILIFE_EAST_CORNER.x;
    const int y = pos.y + JUBILIFE_EAST_CORNER.y;
    return x >= 44 && x <= 48 && y >= 16 && y <= 20;
}

const MapSet& oreburgh_mine_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_OREBURGH_MINE, story_grid("OreburghMine"), {0, 0});
        return maps;
    }();
    return maps;
}

MapSet ravaged_path_maps(const std::vector<TilePos>& smashed){
    TileGrid grid = story_grid("RavagedPath");
    for (const TilePos& rock : smashed){
        grid.set(rock, TileType::WALKABLE);
    }
    MapSet maps;
    maps.add(MAP_RAVAGED_PATH, grid, {0, 0});
    return maps;
}

const MapSet& oreburgh_city_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_OREBURGH_CITY, story_grid("OreburghCity"), {0, 0});
        return maps;
    }();
    return maps;
}

const MapSet& floaroma_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_FLOAROMA_TOWN, story_grid("FloaromaTown"), {0, 0});
        return maps;
    }();
    return maps;
}

const MapSet& route204_north_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_ROUTE_204, story_grid("Route204North"), {0, 0});
        return maps;
    }();
    return maps;
}

const MapSet& route204_south_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_ROUTE_204_SOUTH, story_grid("Route204South"), {0, 0});
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

std::vector<TilePos> story_catch_save_area(){
    const MapSet& maps = story_maps();
    std::vector<TilePos> area;
    for (int y = 25; y <= 29; y++){
        for (int x = 15; x <= 21; x++){
            const TilePos pos = maps.to_world(MAP_ROUTE_202, {x, y});
            if (maps.world().standable(pos)){
                area.push_back(pos);
            }
        }
    }
    return area;
}

std::vector<TilePos> story_after_training_area(){
    const TilePos catch_spot = story_maps().to_world(MAP_ROUTE_202, Route202::CATCH_SPOT);
    std::vector<TilePos> area;
    for (int y = catch_spot.y - 2; y <= catch_spot.y + 2; y++){
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
