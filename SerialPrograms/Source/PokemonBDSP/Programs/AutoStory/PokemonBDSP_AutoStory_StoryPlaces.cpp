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

using Pokemon::Direction;
using Pokemon::TilePos;


namespace{

//  The JSON maps' names and the location banners'. Both halves of Route
//  204 show "Route 204"; no world holds both.
const std::map<std::string, std::string>& banner_names(){
    static const std::map<std::string, std::string> names{
        {"TwinleafTown", "Twinleaf Town"},
        {"Route201", MAP_ROUTE_201},
        {"VerityLakefront", "Verity Lakefront"},
        {"SandgemTown", MAP_SANDGEM_TOWN},
        {"Route202", MAP_ROUTE_202},
        {"JubilifeCity", MAP_JUBILIFE_CITY},
        {"Route203", MAP_ROUTE_203},
        {"OreburghCity", MAP_OREBURGH_CITY},
        {"Route204South", "Route 204"},
        {"Route204North", "Route 204"},
        {"FloaromaTown", MAP_FLOAROMA_TOWN},
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

//  `maps` (JSON names) in one world, each with its tile (0,0) at the corner
//  of its decomp rectangle (devtools/maps/bdsp_maps.py), so maps that meet
//  in the game meet here.
StoryWorld joined(const std::vector<std::pair<const char*, TilePos>>& maps){
    StoryWorld world;
    for (const auto& [map, corner] : maps){
        world.add(banner_name(map), story_map(map), corner);
    }
    return world;
}

//  TODO(#114 multi-map view): MapData has no joined view of maps yet.
//  Twinleaf to Route 202 in one world, at the decomp's world tiles (its
//  cells are 32 tiles square; Route 201's map starts 7 rows into its cell
//  and Route 202's a column west of its own).
const StoryWorld& south_world(){
    static const StoryWorld world = joined({
        {"TwinleafTown", {96, 864}},
        {"Route201", {96, 839}},
        {"VerityLakefront", {64, 832}},
        {"SandgemTown", {160, 832}},
        {"Route202", {159, 800}},
    });
    return world;
}

//  Jubilife without its east column: Route 203's map starts on that column
//  (world x 191, its column 0), and two maps may not both hold open ground.
//  Jubilife's tiles keep their coordinates.
Pokemon::MapData jubilife_west_of_route_203(){
    const Pokemon::MapData& city = story_map("JubilifeCity");
    Pokemon::MapData cut;
    cut.name = city.name;
    cut.source = city.source;
    cut.tiles = Pokemon::Grid<Pokemon::TileKind>{city.tiles.width - 1, city.tiles.height, {}};
    for (int y = 0; y < cut.tiles.height; y++){
        for (int x = 0; x < cut.tiles.width; x++){
            cut.tiles.cells.push_back(city.tiles.at({x, y}));
        }
    }
    for (const Pokemon::MapObject& object : city.objects){
        if (cut.tiles.contains({object.x, object.y})){
            cut.objects.push_back(object);
        }
    }
    return cut;
}

//  01.06: Route 203 west into Jubilife (decomp rectangles (191, 740) and
//  (128, 736)): Route 203's rows 17..20 are Jubilife's street, rows 21..24.
const StoryWorld& route_203_world(){
    static const StoryWorld world = []{
        StoryWorld world = joined({{"Route203", {191, 740}}});
        world.add(MAP_JUBILIFE_CITY, jubilife_west_of_route_203(), {128, 736});
        return world;
    }();
    return world;
}

//  01.08: Jubilife north onto Route 204's south half (160, 704), whose street
//  (12..15, 31) carries on Jubilife's (44..47, 0).
const StoryWorld& route_204_south_world(){
    static const StoryWorld world = joined({{"JubilifeCity", {128, 736}}, {"Route204South", {160, 704}}});
    return world;
}

//  01.10: Route 204's north half (160, 672) up into Floaroma (160, 640).
const StoryWorld& route_204_north_world(){
    static const StoryWorld world = joined({{"Route204North", {160, 672}}, {"FloaromaTown", {160, 640}}});
    return world;
}

//  The Ravaged Path with the rock 01.09 smashes, (22, 43), open: no route
//  before the smash can use it, since it only leads east, where the rocks
//  shut everything else off.
Pokemon::MapData ravaged_path_smashed(){
    Pokemon::MapData map = story_map("RavagedPath");
    Pokemon::cell_at(map.tiles, map_point("RavagedPath", "rocks.13")) = Pokemon::TileKind::OPEN;
    return map;
}

//  The interiors (houses, the lab, the Gym, the Gate, the Ravaged Path),
//  each its own world, named by its JSON map.
const std::map<std::string, StoryWorld>& room_worlds(){
    static const std::map<std::string, StoryWorld> rooms = []{
        std::map<std::string, StoryWorld> ret;
        for (const char* room : {"PlayerHouse2F", "PlayerHouse1F", "RivalHouse1F", "RivalHouse2F", "RowansLab", "OreburghGym", "OreburghGate"}){
            ret[room].add(room, story_map(room), {0, 0});
        }
        ret["RavagedPath"].add("RavagedPath", ravaged_path_smashed(), {0, 0});
        return ret;
    }();
    return rooms;
}

//  A world and the JSON maps it holds.
struct PlanWorld{
    const StoryWorld* world;
    std::set<std::string> maps;
};

//  In order: the first that holds both ends of a walk plans it. Jubilife
//  alone comes before the worlds that join it to a route, so 00.05's walks
//  plan on the city alone.
const std::vector<PlanWorld>& plan_worlds(){
    static const std::vector<PlanWorld> worlds = []{
        std::vector<PlanWorld> ret{
            {&south_world(), {"TwinleafTown", "Route201", "VerityLakefront", "SandgemTown", "Route202"}},
            {&jubilife_maps(), {"JubilifeCity"}},
            {&oreburgh_city_maps(), {"OreburghCity"}},
            {&route_203_world(), {"Route203", "JubilifeCity"}},
            {&route_204_south_world(), {"JubilifeCity", "Route204South"}},
            {&route_204_north_world(), {"Route204North", "FloaromaTown"}},
        };
        for (const auto& [name, world] : room_worlds()){
            ret.push_back({&world, {name}});
        }
        return ret;
    }();
    return worlds;
}

//  Route 203's trainers, from the Diamond decomp's events (zone event
//  narc_0329): where each stands on the JSON map, the way they look and
//  how far. The one by the Gate wanders in BDSP and is off the map.
std::vector<TrainerSight> route_203_trainer_sights(){
    const auto at = [](const char* name){
        for (const Pokemon::MapObject& object : story_map("Route203").objects){
            if (object.name == name){
                return TilePos{object.x, object.y};
            }
        }
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string("Route 203 has no ") + name + ".");
    };
    return {
        {.name = "Youngster (west)", .pos = at("youngster_1"), .facing = Direction::WEST, .range = 2},
        {.name = "Lass (above row 18)", .pos = at("lass_2"), .facing = Direction::WEST, .range = 5},
        {.name = "Youngster (above row 18)", .pos = at("youngster_3"), .facing = Direction::SOUTH, .range = 4},
        {.name = "Lass (by the Gate)", .pos = at("lass_4"), .facing = Direction::SOUTH, .range = 5},
    };
}

//  The trainers of the JSON map `map`, in its own tiles.
std::vector<TrainerSight> map_trainers(const std::string& map){
    if (map == "Route202"){
        return route202_trainer_sights();
    }
    if (map == "Route203"){
        return route_203_trainer_sights();
    }
    if (map == "Route204South"){
        return route204_south_trainer_sights();
    }
    if (map == "Route204North"){
        return route204_north_trainer_sights();
    }
    return {};
}

}


const std::string& story_banner_name(const std::string& map){
    return banner_name(map);
}

const StoryWorld* story_plan_world(const std::string& from, const std::string& to){
    for (const PlanWorld& world : plan_worlds()){
        if (world.maps.contains(from) && world.maps.contains(to)){
            return world.world;
        }
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
    std::vector<TrainerSight> sights;
    for (const PlanWorld& plan : plan_worlds()){
        if (plan.world != &world){
            continue;
        }
        for (const std::string& map : plan.maps){
            for (TrainerSight sight : map_trainers(map)){
                sight.pos = story_to_world(world, map, sight.pos);
                sights.push_back(std::move(sight));
            }
        }
    }
    return sights;
}



}
}
}
