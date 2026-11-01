/*  BDSP AutoStory - Story maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP_AutoStory_StoryMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::MapData;
using Pokemon::TilePos;


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


namespace{

//  One map on its own.
StoryWorld single(const char* name, const std::string& json_map){
    StoryWorld world;
    world.add(name, story_map(json_map), {0, 0});
    return world;
}

//  The tiles of `map` from `corner`, `width` x `height`, and the objects on
//  them, as a map of its own. Its points are left out.
MapData crop(const MapData& map, const TilePos& corner, int width, int height){
    MapData cropped;
    cropped.name = map.name;
    cropped.source = map.source;
    cropped.tiles = Pokemon::Grid<Pokemon::TileKind>{width, height, {}};
    for (int y = 0; y < height; y++){
        for (int x = 0; x < width; x++){
            cropped.tiles.cells.push_back(map.tiles.at({corner.x + x, corner.y + y}));
        }
    }
    for (Pokemon::MapObject object : map.objects){
        object.x -= corner.x;
        object.y -= corner.y;
        if (cropped.tiles.contains({object.x, object.y})){
            cropped.objects.push_back(std::move(object));
        }
    }
    return cropped;
}

bool in_region(const std::string& map, const std::string& region, const TilePos& pos){
    const std::vector<TilePos> tiles = map_region(map, region);
    return std::find(tiles.begin(), tiles.end(), pos) != tiles.end();
}

}


//  Offsets are the maps' places in the game's world grid (the decomp's
//  cells): Route 201 at world (96,839), Sandgem Town at (160,832), Route 202
//  at (159,800). Route 201's row 4 is Sandgem's row 11, below the doors, and
//  Route 202's road comes down off its row 31 onto Sandgem's row 0.
const StoryWorld& story_maps(){
    static const StoryWorld maps = []{
        StoryWorld maps;
        maps.add(MAP_ROUTE_201, story_map("Route201"), {0, 0});
        maps.add(MAP_SANDGEM_TOWN, story_map("SandgemTown"), {64, -7});
        maps.add(MAP_ROUTE_202, story_map("Route202"), {63, -39});
        return maps;
    }();
    return maps;
}

const StoryWorld& jubilife_maps(){
    static const StoryWorld maps = single(MAP_JUBILIFE_CITY, "JubilifeCity");
    return maps;
}

//  Planned one map at a time: Route 203 and Jubilife joined ran for minutes
//  without a plan (r224a, 2026-10-31).
const StoryWorld& route203_maps(){
    static const StoryWorld maps = single(MAP_ROUTE_203, "Route203");
    return maps;
}

bool on_route203_west_line(const TilePos& pos){
    return in_region("Route203", "west_line", pos);
}

//  Columns 42..63, rows 12..25 of Jubilife (world x 170..191, z 748..761),
//  as a map of its own: the cut is the map's edge, which no leg may cross
//  or stop on, not a wall a plan could walk "to".
namespace{
const TilePos JUBILIFE_EAST_CORNER{42, 12};
const int JUBILIFE_EAST_BOTTOM = 25;
}
const StoryWorld& jubilife_east_maps(){
    static const StoryWorld maps = []{
        const MapData& city = story_map("JubilifeCity");
        StoryWorld maps;
        maps.add(MAP_JUBILIFE_CITY, crop(
            city, JUBILIFE_EAST_CORNER, city.tiles.width - JUBILIFE_EAST_CORNER.x, JUBILIFE_EAST_BOTTOM - JUBILIFE_EAST_CORNER.y + 1
        ), {0, 0});
        return maps;
    }();
    return maps;
}

//  Route 203's road (rows 17..19 of its map) meets Jubilife's east street,
//  rows 21..24 of its map, at its east edge; the banner shows a tile or two
//  in. In the corner map's tiles.
const std::vector<TilePos>& jubilife_east_entrance(){
    static const std::vector<TilePos> tiles = []{
        std::vector<TilePos> tiles;
        for (const TilePos& pos : map_region("JubilifeCity", "east_entrance")){
            tiles.push_back({pos.x - JUBILIFE_EAST_CORNER.x, pos.y - JUBILIFE_EAST_CORNER.y});
        }
        return tiles;
    }();
    return tiles;
}

bool on_jubilife_north_road(const TilePos& pos){
    return in_region("JubilifeCity", "north_road", {pos.x + JUBILIFE_EAST_CORNER.x, pos.y + JUBILIFE_EAST_CORNER.y});
}

const StoryWorld& oreburgh_mine_maps(){
    static const StoryWorld maps = single(MAP_OREBURGH_MINE, "OreburghMine");
    return maps;
}

StoryWorld ravaged_path_maps(const std::vector<TilePos>& smashed){
    MapData map = story_map("RavagedPath");
    for (const TilePos& rock : smashed){
        if (map.tiles.contains(rock)){
            Pokemon::cell_at(map.tiles, rock) = Pokemon::TileKind::OPEN;
        }
    }
    StoryWorld maps;
    maps.add(MAP_RAVAGED_PATH, map, {0, 0});
    return maps;
}

const StoryWorld& oreburgh_city_maps(){
    static const StoryWorld maps = single(MAP_OREBURGH_CITY, "OreburghCity");
    return maps;
}

const StoryWorld& floaroma_maps(){
    static const StoryWorld maps = single(MAP_FLOAROMA_TOWN, "FloaromaTown");
    return maps;
}

const StoryWorld& route204_north_maps(){
    static const StoryWorld maps = single(MAP_ROUTE_204, "Route204North");
    return maps;
}

bool in_floaroma_path(const TilePos& pos){
    return in_region("Route204North", "floaroma_path", pos);
}

const StoryWorld& route204_south_maps(){
    static const StoryWorld maps = single(MAP_ROUTE_204_SOUTH, "Route204South");
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
    const StoryWorld& maps = story_maps();
    return {
        maps.to_world(MAP_ROUTE_202, map_point("Route202", "catch_spot")),
        maps.to_world(MAP_ROUTE_202, map_point("Route202", "save_spot")),
    };
}

std::vector<TilePos> story_door_front(const char* map, const TilePos& door){
    const TilePos below = story_maps().to_world(map, door.next(Direction::SOUTH));
    return {below.next(Direction::WEST), below, below.next(Direction::EAST)};
}

namespace{
//  A region of Route 202, in world tiles of story_maps().
std::vector<TilePos> route202_region(const char* region){
    std::vector<TilePos> tiles;
    for (const TilePos& pos : map_region("Route202", region)){
        tiles.push_back(story_maps().to_world(MAP_ROUTE_202, pos));
    }
    return tiles;
}
}

std::vector<TilePos> story_catch_save_area(){
    return route202_region("catch_save_area");
}

std::vector<TilePos> story_after_training_area(){
    return route202_region("after_training_area");
}

std::vector<TrainerSight> story_trainer_sights(){
    std::vector<TrainerSight> sights = route202_trainer_sights();
    for (TrainerSight& sight : sights){
        sight.pos = story_maps().to_world(MAP_ROUTE_202, sight.pos);
    }
    return sights;
}


//  From the events file: each trainer's tile, facing and sight range.
//  Natalie challenged us at (22,22), walking east along the row above the
//  ledge (r75), the fourth tile of her sight.
std::vector<TrainerSight> route202_trainer_sights(){
    return {
        {.name = "tristan", .pos = map_point("Route202", "trainer_tristan"), .facing = Direction::SOUTH, .range = 5},
        {.name = "natalie", .pos = map_point("Route202", "trainer_natalie"), .facing = Direction::SOUTH, .range = 4},
        {.name = "logan", .pos = map_point("Route202", "trainer_logan"), .facing = Direction::SOUTH, .range = 5},
    };
}

std::vector<TrainerSight> route204_north_trainer_sights(){
    const auto at = [](const char* point){ return map_point("Route204North", point); };
    return {
        {.name = "Twins (left)", .pos = at("trainer_twin_left"), .facing = Direction::SOUTH, .range = 3},
        {.name = "Twins (right)", .pos = at("trainer_twin_right"), .facing = Direction::SOUTH, .range = 4},
        {.name = "Aroma Lady Taylor", .pos = at("trainer_taylor"), .facing = Direction::SOUTH, .range = 4},
        {.name = "Aroma Lady Taylor", .pos = at("trainer_taylor"), .facing = Direction::EAST, .range = 4},
        //  Bug Catcher Brandon paces east and west below the flower beds
        //  and stopped us on row 12 (r246a: "Say, what's your opinion on bug
        //  Pokemon?"); the DP map draws him at (20, 12), Platinum's data
        //  has him at (22, 13). The tile is a guess until a shot fixes it.
        {.name = "Bug Catcher Brandon", .pos = at("trainer_brandon"), .facing = Direction::WEST, .range = 4},
        {.name = "Bug Catcher Brandon", .pos = at("trainer_brandon"), .facing = Direction::EAST, .range = 4},
    };
}

//  Sarah turns between north and east; her east line crosses the street's
//  columns, so a walk north meets it or not by luck (r233a passed, r234a
//  fought). The planner cannot avoid it: she is listed so the route knows
//  whom it may meet, with the range that fits.
std::vector<TrainerSight> route204_south_trainer_sights(){
    const auto at = [](const char* point){ return map_point("Route204South", point); };
    return {
        {.name = "Lass Samantha", .pos = at("trainer_samantha"), .facing = Direction::SOUTH, .range = 4},
        {.name = "Lass Sarah", .pos = at("trainer_sarah"), .facing = Direction::NORTH, .range = 4},
        {.name = "Lass Sarah", .pos = at("trainer_sarah"), .facing = Direction::EAST, .range = 4},
        {.name = "Youngster Tyler", .pos = at("trainer_tyler"), .facing = Direction::EAST, .range = 4},
        {.name = "Youngster Tyler", .pos = at("trainer_tyler"), .facing = Direction::WEST, .range = 4},
    };
}

//  Both see the middle column, so a walk north from the entrance is
//  stopped twice before Roark (01.04's record).
std::vector<TrainerSight> oreburgh_gym_trainer_sights(){
    return {
        {.name = "Youngster (first tier)", .pos = map_point("OreburghGym", "trainer_first_tier"), .facing = Direction::EAST, .range = 3},
        {.name = "Youngster (second tier)", .pos = map_point("OreburghGym", "trainer_second_tier"), .facing = Direction::WEST, .range = 4},
    };
}

std::vector<TilePos> route204_south_tiles_to_avoid(){
    std::vector<TilePos> tiles;
    for (const TrainerSight& trainer : route204_south_trainer_sights()){
        if (trainer.name == "Lass Sarah" && trainer.facing == Direction::EAST){
            continue;
        }
        for (const TilePos& pos : sight_tiles(route204_south_maps(), trainer)){
            tiles.push_back(pos);
        }
    }
    return tiles;
}



}
}
}
