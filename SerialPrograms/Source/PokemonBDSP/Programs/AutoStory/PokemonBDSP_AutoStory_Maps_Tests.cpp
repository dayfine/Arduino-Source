/*  BDSP AutoStory Maps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The built-in story maps (Data/PokemonBDSP_AutoStory_Maps.h): every map
 *  loads, its points and objects stand where they can, and the lookups
 *  find what the story code asks for.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP_AutoStory_Maps_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::MapData;
using Pokemon::MapObject;
using Pokemon::TileKind;
using Pokemon::TilePos;

bool is_ground(TileKind tile){
    return tile == TileKind::OPEN || tile == TileKind::TALL_GRASS;
}

//  The person or item that stands still on `pos`, if any.
const MapObject* standing_object(const MapData& map, const TilePos& pos){
    for (const MapObject& object : map.objects){
        if (!object.wander && object.x == pos.x && object.y == pos.y){
            return &object;
        }
    }
    return nullptr;
}

//  "rocks" for "rocks.3", the point's own name otherwise.
std::string region_of(const std::string& point){
    const size_t dot = point.rfind('.');
    if (dot == std::string::npos || dot + 1 == point.size()){
        return point;
    }
    const bool numbered = std::all_of(point.begin() + dot + 1, point.end(), [](char c){ return '0' <= c && c <= '9'; });
    return numbered ? point.substr(0, dot) : point;
}


//  Every built-in map parses through the shared loader, named as its file.
class Test_StoryMapsLoad : public UnitTest{
public:
    Test_StoryMapsLoad()
        : UnitTest("PokemonBDSP::AutoStory - Maps: the built-in JSON maps load")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        if (bdsp_map_json().empty()){
            return std::string("No built-in maps.");
        }
        for (const auto& [name, json] : bdsp_map_json()){
            try{
                const MapData& map = story_map(name);
                if (map.name != name){
                    return std::format("{}.json: story_map() returned \"{}\".", name, map.name);
                }
                logger.log(std::format(
                    "{}: {} x {}, {} points, {} objects, {} patches",
                    name, map.tiles.width, map.tiles.height, map.points.size(), map.objects.size(), map.patches.size()
                ));
            }catch (Exception& e){
                return std::format("{}: {}: {}", name, e.name(), e.message());
            }
        }
        return true;
    }
};


//  What a named point must be on. Points not listed are ground the player
//  stands on, with nobody standing there.
enum class PointOn{
    DOOR,
    SIGN,
    PERSON,     //  someone stands still there
    WALL,       //  a rock until smashed
};
struct PointCase{
    std::string map;
    std::string point;      //  a point, or a region for all its tiles
    PointOn on;
};

const std::vector<PointCase>& point_cases(){
    static const std::vector<PointCase> cases{
        {.map = "Route201", .point = "border_sign", .on = PointOn::SIGN},
        {.map = "Route202", .point = "trainer_tristan", .on = PointOn::PERSON},
        {.map = "Route202", .point = "trainer_natalie", .on = PointOn::PERSON},
        {.map = "Route202", .point = "trainer_logan", .on = PointOn::PERSON},
        {.map = "Route204North", .point = "trainer_twin_left", .on = PointOn::PERSON},
        {.map = "Route204North", .point = "trainer_twin_right", .on = PointOn::PERSON},
        {.map = "Route204North", .point = "trainer_taylor", .on = PointOn::PERSON},
        {.map = "Route204North", .point = "trainer_brandon", .on = PointOn::PERSON},
        {.map = "Route204South", .point = "trainer_samantha", .on = PointOn::PERSON},
        {.map = "Route204South", .point = "trainer_sarah", .on = PointOn::PERSON},
        {.map = "Route204South", .point = "trainer_tyler", .on = PointOn::PERSON},
        {.map = "SandgemTown", .point = "lab_door", .on = PointOn::DOOR},
        {.map = "SandgemTown", .point = "pokemon_center_door", .on = PointOn::DOOR},
        {.map = "SandgemTown", .point = "pokemon_center_sign", .on = PointOn::SIGN},
        {.map = "SandgemTown", .point = "poke_mart_door", .on = PointOn::DOOR},
        {.map = "JubilifeCity", .point = "trainers_school_door", .on = PointOn::DOOR},
        {.map = "JubilifeCity", .point = "pokemon_center_door", .on = PointOn::DOOR},
        {.map = "JubilifeCity", .point = "tv_station_door", .on = PointOn::DOOR},
        {.map = "JubilifeCity", .point = "poketch_president", .on = PointOn::PERSON},
        {.map = "JubilifeCity", .point = "clown_mart", .on = PointOn::PERSON},
        {.map = "JubilifeCity", .point = "clown_tv_station", .on = PointOn::PERSON},
        {.map = "JubilifeCity", .point = "clown_poketch_company", .on = PointOn::PERSON},
        {.map = "OreburghCity", .point = "pokemon_center_door", .on = PointOn::DOOR},
        {.map = "OreburghCity", .point = "gym_door", .on = PointOn::DOOR},
        {.map = "OreburghCity", .point = "gate_mouth", .on = PointOn::DOOR},
        {.map = "OreburghGym", .point = "roark", .on = PointOn::PERSON},
        {.map = "OreburghGym", .point = "trainer_first_tier", .on = PointOn::PERSON},
        {.map = "OreburghGym", .point = "trainer_second_tier", .on = PointOn::PERSON},
        {.map = "OreburghMine", .point = "roark", .on = PointOn::PERSON},
        {.map = "RavagedPath", .point = "rocks", .on = PointOn::WALL},
        {.map = "FloaromaTown", .point = "old_woman", .on = PointOn::PERSON},
        {.map = "FloaromaTown", .point = "old_man", .on = PointOn::PERSON},
        {.map = "FloaromaTown", .point = "pokemon_center_door", .on = PointOn::DOOR},
        {.map = "TwinleafTown", .point = "player_house_door", .on = PointOn::DOOR},
        {.map = "TwinleafTown", .point = "rival_house_door", .on = PointOn::DOOR},
        {.map = "PlayerHouse1F", .point = "mom", .on = PointOn::PERSON},
        {.map = "PlayerHouse1F", .point = "door", .on = PointOn::DOOR},
        {.map = "RivalHouse1F", .point = "door", .on = PointOn::DOOR},
        {.map = "RowansLab", .point = "door", .on = PointOn::DOOR},
    };
    return cases;
}

//  An error naming what `pos` should be, or empty when it is.
std::string check_point_on(const MapData& map, const TilePos& pos, PointOn on){
    const TileKind tile = map.tiles.at(pos);
    const MapObject* object = standing_object(map, pos);
    switch (on){
    case PointOn::DOOR:
        return tile == TileKind::DOOR ? "" : "not a door";
    case PointOn::SIGN:
        return tile == TileKind::SIGN ? "" : "not a sign";
    case PointOn::PERSON:
        return is_ground(tile) && object != nullptr && object->kind == Pokemon::ObjectKind::NPC ? "" : "nobody stands still there";
    case PointOn::WALL:
        return tile == TileKind::WALL ? "" : "not a wall";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown PointOn.");
}

std::string check_ground(const MapData& map, const TilePos& pos){
    if (!is_ground(map.tiles.at(pos))){
        return std::format("'{}', not ground", Pokemon::glyph_of(map.tiles.at(pos)));
    }
    return standing_object(map, pos) == nullptr ? "" : "someone stands there";
}


//  Every point is ground with nobody on it, unless point_cases() says
//  otherwise; every numbered region runs from 1 with no gap; every case
//  names a point the map has; every warp leads to a point that exists.
class Test_StoryMapPoints : public UnitTest{
public:
    Test_StoryMapPoints()
        : UnitTest("PokemonBDSP::AutoStory - Maps: points stand where they can")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        for (const PointCase& item : point_cases()){
            const MapData& map = story_map(item.map);
            if (!map.points.contains(item.point) && !map.points.contains(item.point + ".1")){
                return std::format("{}: no point or region \"{}\".", item.map, item.point);
            }
        }
        size_t checked = 0;
        for (const auto& [name, json] : bdsp_map_json()){
            const MapData& map = story_map(name);
            for (const auto& [point, pos] : map.points){
                const std::string region = region_of(point);
                //  map_region() counts from 1 and stops at a gap.
                if (region != point && std::stoul(point.substr(region.size() + 1)) > map_region(name, region).size()){
                    return std::format("{}: {} is not reached counting {}.1, {}.2, ...", name, point, region, region);
                }
                const auto it = std::ranges::find_if(point_cases(), [&](const PointCase& item){
                    return item.map == name && item.point == region;
                });
                const std::string error = it == point_cases().end() ? check_ground(map, pos) : check_point_on(map, pos, it->on);
                if (!error.empty()){
                    return std::format("{}: {} {}: {}.", name, point, Pokemon::to_string(pos), error);
                }
                checked++;
            }
            for (const Pokemon::MapWarp& warp : map.warps){
                try{
                    map_point(warp.to_map, warp.to_point);
                }catch (InternalProgramError& e){
                    return std::format("{}: the warp at {}: {}", name, Pokemon::to_string(TilePos{warp.x, warp.y}), e.message());
                }
            }
        }
        logger.log(std::format("{} points checked.", checked));
        return true;
    }
};


//  Everyone stands on ground, and a wanderer's box holds their tile.
class Test_StoryMapObjects : public UnitTest{
public:
    Test_StoryMapObjects()
        : UnitTest("PokemonBDSP::AutoStory - Maps: objects stand on open ground")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        size_t checked = 0;
        for (const auto& [name, json] : bdsp_map_json()){
            const MapData& map = story_map(name);
            for (const MapObject& object : map.objects){
                const TilePos pos{object.x, object.y};
                if (!is_ground(map.tiles.at(pos))){
                    return std::format("{}: {} stands on '{}' at {}.", name, object.name, Pokemon::glyph_of(map.tiles.at(pos)), Pokemon::to_string(pos));
                }
                const auto& box = object.wander;
                if (box && (pos.x < box->x0 || pos.x > box->x1 || pos.y < box->y0 || pos.y > box->y1)){
                    return std::format("{}: {} at {} is outside their wander box.", name, object.name, Pokemon::to_string(pos));
                }
                checked++;
            }
        }
        logger.log(std::format("{} objects checked.", checked));
        return true;
    }
};


//  map_point() and map_region() find what the story asks for, regions in
//  order, and throw for what a map does not have.
struct LookupCase{
    std::string name;
    std::function<std::vector<TilePos>()> lookup;
    //  The tiles, or empty when the lookup must throw InternalProgramError.
    std::vector<TilePos> expected;
};

class Test_StoryMapLookups : public UnitTest{
public:
    Test_StoryMapLookups()
        : UnitTest("PokemonBDSP::AutoStory - Maps: map_point() and map_region()")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<LookupCase> cases{
            {
                .name = "a point",
                .lookup = []{ return std::vector<TilePos>{map_point("SandgemTown", "pokemon_center_door")}; },
                .expected = {{17, 10}},
            },
            {
                .name = "a region, in order",
                .lookup = []{ return map_region("Route204North", "floaroma_waypoints"); },
                .expected = {{20, 27}, {11, 27}, {11, 22}, {19, 22}, {19, 12}, {11, 12}, {11, 5}},
            },
            {
                .name = "an unknown map",
                .lookup = []{ return std::vector<TilePos>{map_point("Route299", "door")}; },
                .expected = {},
            },
            {
                .name = "an unknown point",
                .lookup = []{ return std::vector<TilePos>{map_point("SandgemTown", "gym_door")}; },
                .expected = {},
            },
            {
                .name = "a point is not a region",
                .lookup = []{ return map_region("SandgemTown", "lab_door"); },
                .expected = {},
            },
        };
        for (const LookupCase& item : cases){
            std::vector<TilePos> tiles;
            try{
                tiles = item.lookup();
            }catch (InternalProgramError&){
                if (item.expected.empty()){
                    continue;
                }
                return item.name + ": threw InternalProgramError.";
            }
            if (tiles != item.expected){
                std::string read;
                for (const TilePos& pos : tiles){
                    read += " " + Pokemon::to_string(pos);
                }
                return std::format("{}: read{}, expected {} tiles{}.", item.name, read, item.expected.size(),
                    item.expected.empty() ? " (an InternalProgramError)" : "");
            }
        }
        return true;
    }
};

}


void add_tests_AutoStory_Maps(UnitTestDatabase& database){
    database.add<Test_StoryMapsLoad>();
    database.add<Test_StoryMapPoints>();
    database.add<Test_StoryMapObjects>();
    database.add<Test_StoryMapLookups>();
}


}
}
}
