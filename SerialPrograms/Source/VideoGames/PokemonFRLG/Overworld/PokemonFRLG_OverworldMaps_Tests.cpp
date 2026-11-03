/*  Overworld Maps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_OverworldMaps : public UnitTest{
public:
    Test_OverworldMaps(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::OverworldMaps - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};


const char* const VALID_MAP = R"({
    "name": "Test",
    "source": {"decomp_file": "data/maps/Test/map.json", "revision": "test", "date": "2026-10-31"},
    "tiles": ["#.w", "~D."],
    "elevation": ["031", "3f3"],
    "points": {"door": [1, 1]},
    "warps": [
        {"x": 1, "y": 1, "enter": "walk_into", "to_map": "Other", "to_point": "warp_2"},
        {"x": 2, "y": 1, "enter": "walk_south"}
    ],
    "objects": [{"name": "nurse", "kind": "npc", "x": 0, "y": 1, "wander": {"x0": 0, "y0": 1, "x1": 1, "y1": 1}, "hide_flag": "FLAG_X"}],
    "flag_sets": {"later": ["FLAG_X", "FLAG_Y"]},
    "connections": [{"side": "south", "to_map": "Other", "offset": -4}]
})";

//  The ParseException that parsing `json` throws, or empty if it doesn't.
std::string parse_error(const std::string& json){
    try{
        parse_map(json, "Test.json");
    }catch (ParseException& e){
        return e.message();
    }
    return "";
}

//  Whether `text` contains every one of `parts`.
bool mentions(const std::string& text, std::initializer_list<const char*> parts){
    for (const char* part : parts){
        if (text.find(part) == std::string::npos){
            return false;
        }
    }
    return true;
}

UnitTestResult test_valid_map(){
    const MapData map = parse_map(VALID_MAP, "Test.json");
    if (map.name != "Test" || map.tiles.width != 3 || map.tiles.height != 2
        || map.tiles.at({0, 1}) != TileKind::TALL_GRASS || exit_mat(map, {2, 1}) != Direction::SOUTH
    ){
        return "Wrong name, size or tiles.";
    }
    if (elevation_at(map, {2, 0}) != 1 || elevation_at(map, {1, 1}) != 15){
        return "Wrong elevation.";
    }
    const MapWarp* door = warp_at(map, {1, 1});
    if (map.warps.size() != 2 || door == nullptr || !door->to || door->to->map != "Other" || door->to->point != "warp_2"){
        return "Wrong warps.";
    }
    const MapObject* nurse = object_at(map, {1, 1});
    if (map.objects.size() != 1 || nurse == nullptr || nurse->name != "nurse" || nurse->hide_flag != "FLAG_X"){
        return "Wrong objects.";
    }
    const MapConnection& connection = map.connections.at(0);
    if (connection.side != Direction::SOUTH || connection.offset != -4 || connection.to_map != "Other"){
        return "Wrong connections.";
    }
    if (map.points.at("door") != TilePos{1, 1} || map.flag_sets.at("later") != std::set<std::string>{"FLAG_X", "FLAG_Y"}){
        return "Wrong points or flag sets.";
    }
    return true;
}

UnitTestResult test_missing_field(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"tiles\""), 7, "\"cells\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "tiles", "missing"})){
        return "Expected an error naming the missing tiles, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_elevation_left_out(){
    std::string json = VALID_MAP;
    json.erase(json.find("\"elevation\""), std::string("\"elevation\": [\"031\", \"3f3\"],").size());
    const MapData map = parse_map(json, "Test.json");
    for (int y = 0; y < 2; y++){
        for (int x = 0; x < 3; x++){
            if (elevation_at(map, {x, y}) != ELEVATION_GROUND){
                return "Expected ground everywhere without an elevation grid.";
            }
        }
    }
    return true;
}

UnitTestResult test_bad_field_type(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"x\": 1"), 6, "\"x\": \"1\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "warps[0]", "x", "integer"})){
        return "Expected an error naming warps[0].x, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_bad_row_length(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"~D.\""), 5, "\"~D\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "tiles[1]", "string of 3 glyphs"})){
        return "Expected an error naming the short row, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_unknown_tile(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"#.w\""), 5, "\"#.?\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "tiles[0]", "unknown glyph '?'", "(2, 0)"})){
        return "Expected an error naming the unknown tile, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_not_json(){
    const std::string error = parse_error("{\"name\": ");
    if (!mentions(error, {"Test.json", "not a JSON object"})){
        return "Expected an error for broken JSON, got: \"" + error + "\"";
    }
    return true;
}

const MapData* built_in(const std::vector<MapData>& maps, const std::string& name){
    for (const MapData& map : maps){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

//  Route 3, Route 4 and its Pokemon Center load, joined to Pewter City; the
//  Center doesn't let the player run, the routes do.
UnitTestResult test_route3_route4_maps(){
    const std::vector<MapData> maps = overworld_maps();
    for (const char* name : {"Route3", "Route4", "Route4_PokemonCenter_1F"}){
        if (built_in(maps, name) == nullptr){
            return std::string("Missing map ") + name + ".";
        }
    }
    if (!built_in(maps, "Route3")->allow_running || !built_in(maps, "PewterCity")->allow_running
        || built_in(maps, "Route4_PokemonCenter_1F")->allow_running || built_in(maps, "PewterCity_PokemonCenter_1F")->allow_running
    ){
        return "Expected running on the routes and Pewter City, not in the Pokemon Centers.";
    }
    const MapPosition nurse{"Route4_PokemonCenter_1F", map_point(maps, "Route4_PokemonCenter_1F", "nurse_front")};
    const MapPosition salesman{"Route4_PokemonCenter_1F", map_point(maps, "Route4_PokemonCenter_1F", "salesman_front")};
    if (nurse.point != TilePos{7, 4} || salesman.point != TilePos{2, 3}){
        return "Unexpected nurse_front or salesman_front.";
    }
    const MapPosition pewter{"PewterCity", {45, 21}};
    if (!plan_overworld_route(maps, pewter, RouteGoal{nurse, Direction::NORTH})){
        return "No route from Pewter City over Route 3 to Route 4's Pokemon Center.";
    }
    return true;
}

//  A planned step walks where its map doesn't let the player run, and only
//  there: out of Pewter's Pokemon Center to the Gym.
UnitTestResult test_walk_only_indoors(){
    const std::vector<MapData> maps = overworld_maps();
    MapPosition from{"PewterCity_PokemonCenter_1F", map_point(maps, "PewterCity_PokemonCenter_1F", "nurse_front")};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps, from, RouteGoal{MapPosition{"PewterCity", map_point(maps, "PewterCity", "gym_front")}, Direction::NORTH}
    );
    if (!route){
        return "No route from Pewter's Pokemon Center to the Gym.";
    }
    bool indoors = false;
    bool outdoors = false;
    for (const RouteStep& step : *route){
        const bool in_center = from.map == "PewterCity_PokemonCenter_1F";
        if (step.action == RouteAction::WALK && step.walk_only != in_center){
            return "Wrong walk_only on " + to_string(step) + ", from " + to_string(from) + ".";
        }
        indoors = indoors || in_center;
        outdoors = outdoors || !in_center;
        from = step.end;
    }
    if (!indoors || !outdoors){
        return "Expected steps inside the Center and outside.";
    }
    const std::optional<WallHold> hold = hold_to_wall(maps, {"PewterCity_PokemonCenter_1F", {7, 4}}, false, Direction::WEST);
    if (!hold || !hold->walk_only){
        return "Expected a hold to a wall in the Center to walk.";
    }
    return true;
}

}


void add_tests_OverworldMaps(UnitTestDatabase& database){
    database.add<Test_OverworldMaps>("valid map", test_valid_map);
    database.add<Test_OverworldMaps>("missing field", test_missing_field);
    database.add<Test_OverworldMaps>("elevation left out", test_elevation_left_out);
    database.add<Test_OverworldMaps>("field of the wrong type", test_bad_field_type);
    database.add<Test_OverworldMaps>("row of the wrong length", test_bad_row_length);
    database.add<Test_OverworldMaps>("unknown tile", test_unknown_tile);
    database.add<Test_OverworldMaps>("not JSON", test_not_json);
    database.add<Test_OverworldMaps>("Route 3, Route 4 and its Pokemon Center", test_route3_route4_maps);
    database.add<Test_OverworldMaps>("walks where running isn't allowed", test_walk_only_indoors);
}



}
}
}
