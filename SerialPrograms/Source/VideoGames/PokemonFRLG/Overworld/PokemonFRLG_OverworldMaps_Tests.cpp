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

struct OverworldMapsInput{
    std::string name;
    UnitTestResult (*body)();
};

struct OverworldMapsExpectation{
    const char* description;
};

struct OverworldMapsTestCase{
    OverworldMapsInput input;
    OverworldMapsExpectation expectation;
};

class Test_OverworldMaps : public UnitTest{
public:
    explicit Test_OverworldMaps(OverworldMapsTestCase scenario)
        : UnitTest("PokemonFRLG::OverworldMaps - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_scenario.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    OverworldMapsTestCase m_scenario;
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

//  Route 3's Lass Robin, shrunk: she wanders x 2, y 2..4 looking north and
//  south, sight 3, below a corridor (row 1) walled off at x 2 above it; a
//  bug catcher at (5, 4) looks west, sight 2.
const char* const TRAINERS_MAP = R"({
    "name": "Trainers",
    "source": {"decomp_file": "", "revision": "test", "date": "2026-11-02"},
    "tiles": ["##.####", ".......", ".......", ".......", ".......", "~~~~~~~"],
    "objects": [
        {"name": "lass", "kind": "npc", "x": 2, "y": 3, "wander": {"x0": 2, "y0": 2, "x1": 2, "y1": 4}, "trainer": {"sight": 3, "looks": ["north", "south"]}},
        {"name": "bug_catcher", "kind": "npc", "x": 5, "y": 4, "facing": "west", "trainer": {"sight": 2}},
        {"name": "youngster", "kind": "npc", "x": 6, "y": 1}
    ]
})";

//  A wanderer sees from every tile she may stand on, short of a wall; a
//  trainer standing still only along his facing; a non-trainer nothing.
UnitTestResult test_trainer_sight(){
    const MapData map = parse_map(TRAINERS_MAP, "Trainers.json");
    const MapObject& lass = map.objects[0];
    const MapObject& bug_catcher = map.objects[1];
    const MapObject& youngster = map.objects[2];
    struct Case{
        const MapObject* object;
        TilePos tile;
        bool sees;
    };
    const Case cases[] = {
        {&lass, {2, 1}, true},          //  from (2, 2..4), north
        {&lass, {2, 0}, true},          //  the gap in the wall, 2 tiles up from (2, 2)
        {&lass, {2, 5}, true},          //  south, from (2, 4)
        {&lass, {1, 1}, false},
        {&bug_catcher, {3, 4}, true},
        {&bug_catcher, {2, 4}, false},  //  past his sight
        {&bug_catcher, {5, 3}, false},  //  not his way
        {&youngster, {5, 1}, false},
    };
    for (const Case& c : cases){
        if (trainer_may_see(map, *c.object, c.tile) != c.sees){
            return c.object->name + (c.sees ? " should see " : " shouldn't see ") + Pokemon::to_string(c.tile) + ".";
        }
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

//  A hall with an escalator at (1, 2) to a floor that isn't loaded, like
//  Route 4's Pokemon Center.
const char* const ESCALATOR_HALL = R"({
    "name": "Hall", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-02"},
    "tiles": ["######", "#....#", "#....#", "######"],
    "warps": [{"x": 1, "y": 2, "enter": "step_on"}]
})";

//  Holding west along the escalator's row rides it: a leg there ends by
//  counting tiles, never by holding to the wall behind it (console B,
//  2026-11-02, Route 4's Pokemon Center).
UnitTestResult test_no_hold_onto_escalator(){
    const std::vector<MapData> maps{parse_map(ESCALATOR_HALL, "Hall")};
    if (hold_to_wall(maps, {"Hall", {4, 2}}, false, Direction::WEST)){
        return "Expected no hold west along the escalator's row.";
    }
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, {"Hall", {4, 2}}, {{"Hall", {2, 1}}, std::nullopt});
    if (!route || !(follow_overworld_route(maps, {"Hall", {4, 2}}, *route) == MapPosition{"Hall", {2, 1}})){
        return "Expected a route from (4, 2) to (2, 1).";
    }
    for (const RouteStep& step : *route){
        if (step.to_wall && step.direction == Direction::WEST && step.end.point == TilePos{2, 2}){
            return "Expected no hold west next to the escalator: " + to_string(step);
        }
    }
    return true;
}


const OverworldMapsTestCase OVERWORLD_MAPS_CASES[] = {
    {
        .input = {.name = "valid map", .body = test_valid_map},
        .expectation = {.description = "parses valid map json and properties"},
    },
    {
        .input = {.name = "missing field", .body = test_missing_field},
        .expectation = {.description = "detects missing required fields in map json"},
    },
    {
        .input = {.name = "elevation left out", .body = test_elevation_left_out},
        .expectation = {.description = "handles omitted elevation data cleanly"},
    },
    {
        .input = {.name = "field of the wrong type", .body = test_bad_field_type},
        .expectation = {.description = "rejects invalid field types in map json"},
    },
    {
        .input = {.name = "row of the wrong length", .body = test_bad_row_length},
        .expectation = {.description = "rejects rows not matching declared width"},
    },
    {
        .input = {.name = "unknown tile", .body = test_unknown_tile},
        .expectation = {.description = "rejects unmapped character codes in tile map"},
    },
    {
        .input = {.name = "not JSON", .body = test_not_json},
        .expectation = {.description = "rejects malformed json input string"},
    },
    {
        .input = {.name = "trainer sight", .body = test_trainer_sight},
        .expectation = {.description = "evaluates line of sight for trainer objects"},
    },
    {
        .input = {.name = "no hold onto an escalator", .body = test_no_hold_onto_escalator},
        .expectation = {.description = "prevents holding into an escalator"},
    },
};

} // namespace


void add_tests_OverworldMaps(UnitTestDatabase& database){
    for (const auto& scenario : OVERWORLD_MAPS_CASES){
        database.add<Test_OverworldMaps>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
