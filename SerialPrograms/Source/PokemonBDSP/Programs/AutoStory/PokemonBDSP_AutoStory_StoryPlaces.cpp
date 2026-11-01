/*  BDSP AutoStory - Story places
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <set>
#include "Common/Cpp/Exceptions.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::TilePos;


namespace{

//  The JSON maps' names and the location banners'.
const std::map<std::string, std::string>& banner_names(){
    static const std::map<std::string, std::string> names{
        {"TwinleafTown", "Twinleaf Town"},
        {"Route201", MAP_ROUTE_201},
        {"VerityLakefront", "Verity Lakefront"},
        {"SandgemTown", MAP_SANDGEM_TOWN},
        {"Route202", MAP_ROUTE_202},
        {"JubilifeCity", MAP_JUBILIFE_CITY},
    };
    return names;
}

const std::string& banner_name(const std::string& map){
    const auto& names = banner_names();
    if (auto iter = names.find(map); iter != names.end()){
        return iter->second;
    }
    return map;
}

//  Twinleaf to Route 202 in one world, at the decomp's world tiles (its
//  cells are 32 tiles square; Route 201's map starts 7 rows into its cell
//  and Route 202's a column west of its own).
const StoryWorld& south_world(){
    static const StoryWorld world = []{
        StoryWorld world;
        world.add(banner_name("TwinleafTown"), story_map("TwinleafTown"), {96, 864});
        world.add(banner_name("Route201"), story_map("Route201"), {96, 839});
        world.add(banner_name("VerityLakefront"), story_map("VerityLakefront"), {64, 832});
        world.add(banner_name("SandgemTown"), story_map("SandgemTown"), {160, 832});
        world.add(banner_name("Route202"), story_map("Route202"), {159, 800});
        return world;
    }();
    return world;
}

//  The interiors of 00.01 to 00.04, each its own world.
const std::map<std::string, StoryWorld>& room_worlds(){
    static const std::map<std::string, StoryWorld> rooms = []{
        std::map<std::string, StoryWorld> ret;
        for (const char* room : {"PlayerHouse2F", "PlayerHouse1F", "RivalHouse1F", "RivalHouse2F", "RowansLab"}){
            ret[room].add(room, story_map(room), {0, 0});
        }
        return ret;
    }();
    return rooms;
}

bool on_world(const StoryWorld& world, const std::string& map){
    if (&world == &south_world()){
        static const std::set<std::string> south{"TwinleafTown", "Route201", "VerityLakefront", "SandgemTown", "Route202"};
        return south.contains(map);
    }
    return false;
}

//  The tiles of Route 202 we can stand on in the box (x0, y0) to (x1, y1).
std::vector<TilePos> route202_open(int x0, int y0, int x1, int y1){
    const StoryWorld& world = south_world();
    std::vector<TilePos> tiles;
    for (int y = y0; y <= y1; y++){
        for (int x = x0; x <= x1; x++){
            if (world.standable(story_to_world(world, "Route202", {x, y}))){
                tiles.push_back({x, y});
            }
        }
    }
    return tiles;
}

std::vector<TilePos> route202_region(const std::string& region){
    const TilePos catch_spot = map_point("Route202", "catch_spot");
    if (region == "catch_save_area"){
        //  Wherever the catch's walks in the grass ended (story_catch_save_area()).
        return route202_open(15, 25, 21, 29);
    }
    if (region == "after_training_area"){
        //  The catch spot's column and one either side, two rows up and down.
        return route202_open(catch_spot.x - 1, catch_spot.y - 2, catch_spot.x + 1, catch_spot.y + 2);
    }
    if (region == "catch_grass"){
        //  The training area's tall grass, which training walks into.
        std::vector<TilePos> grass;
        for (const TilePos& pos : route202_region("after_training_area")){
            if (story_map("Route202").tiles.at(pos) == Pokemon::TileKind::TALL_GRASS){
                grass.push_back(pos);
            }
        }
        return grass;
    }
    if (region == "jubilife_exit"){
        //  The open rows at the top: north from any of them is Jubilife.
        return route202_open(0, 0, story_map("Route202").tiles.width - 1, 3);
    }
    return {};
}

}


TilePos story_point(const std::string& map, const std::string& point){
    return map_point(map, point);
}

std::vector<TilePos> story_region(const std::string& map, const std::string& region){
    if (map == "Route202"){
        std::vector<TilePos> tiles = route202_region(region);
        if (!tiles.empty()){
            return tiles;
        }
    }
    return map_region(map, region);
}

const StoryWorld* story_plan_world(const std::string& from, const std::string& to){
    if (on_world(south_world(), from) && on_world(south_world(), to)){
        return &south_world();
    }
    if (from == "JubilifeCity" && to == from){
        return &jubilife_maps();
    }
    const auto& rooms = room_worlds();
    if (auto iter = rooms.find(from); iter != rooms.end() && to == from){
        return &iter->second;
    }
    return nullptr;
}

TilePos story_to_world(const StoryWorld& world, const std::string& map, const TilePos& local){
    return world.to_world(banner_name(map), local);
}

std::optional<TilePos> story_to_local(const StoryWorld& world, const std::string& map, const TilePos& pos){
    const std::optional<MapTile> tile = world.to_local(pos);
    if (!tile || tile->map != banner_name(map)){
        return std::nullopt;
    }
    return tile->pos;
}

std::vector<TrainerSight> story_trainers(const StoryWorld& world){
    if (!on_world(world, "Route202")){
        return {};
    }
    std::vector<TrainerSight> sights = route202_trainer_sights();
    for (TrainerSight& sight : sights){
        sight.pos = story_to_world(world, "Route202", sight.pos);
    }
    return sights;
}



}
}
}
