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

}


void add_tests_OverworldMaps(UnitTestDatabase& database){
    database.add<Test_OverworldMaps>("valid map", test_valid_map);
    database.add<Test_OverworldMaps>("missing field", test_missing_field);
    database.add<Test_OverworldMaps>("elevation left out", test_elevation_left_out);
    database.add<Test_OverworldMaps>("field of the wrong type", test_bad_field_type);
    database.add<Test_OverworldMaps>("row of the wrong length", test_bad_row_length);
    database.add<Test_OverworldMaps>("unknown tile", test_unknown_tile);
    database.add<Test_OverworldMaps>("not JSON", test_not_json);
}



}
}
}
