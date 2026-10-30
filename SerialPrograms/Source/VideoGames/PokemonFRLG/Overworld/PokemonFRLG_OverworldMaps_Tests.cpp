/*  Overworld Maps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <functional>
#include <initializer_list>
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
    "name": "Test", "width": 3, "height": 2,
    "tiles": ["#.~", "\"Dv"],
    "elevation": ["031", "3f3"],
    "warps": [{"x": 1, "y": 1, "dest_map": "Other", "dest_warp": 2}],
    "objects": [{"x": 0, "y": 1, "kind": "nurse", "flag": "FLAG_X", "wander_x": 1, "wander_y": 0}],
    "connections": [{"direction": "south", "offset": -4, "map": "Other"}]
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
        || map.tiles.at({0, 1}) != Tile::TALL_GRASS || map.tiles.at({2, 1}) != Tile::MAT_SOUTH
    ){
        return "Wrong name, size or tiles.";
    }
    if (elevation_at(map, {2, 0}) != 1 || elevation_at(map, {1, 1}) != 15){
        return "Wrong elevation.";
    }
    if (map.warps.size() != 1 || map.warps[0].dest_map != "Other" || map.warps[0].dest_warp != 2){
        return "Wrong warps.";
    }
    const MapObject* nurse = object_at(map, {1, 1});
    if (map.objects.size() != 1 || nurse == nullptr || nurse->kind != "nurse" || nurse->flag != "FLAG_X"){
        return "Wrong objects.";
    }
    const MapConnection& connection = map.connections.at(0);
    if (connection.direction != Direction::SOUTH || connection.offset != -4 || connection.map != "Other"){
        return "Wrong connections.";
    }
    return true;
}

UnitTestResult test_missing_field(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"elevation\""), 11, "\"heights\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "elevation", "missing"})){
        return "Expected an error naming the missing elevation, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_bad_field_type(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"dest_warp\": 2"), 14, "\"dest_warp\": \"2\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "warps[0]", "dest_warp", "integer"})){
        return "Expected an error naming warps[0].dest_warp, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_bad_row_length(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"#.~\""), 5, "\"#.\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "tiles[0]", "string of 3 tiles"})){
        return "Expected an error naming the short row, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_unknown_tile(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"#.~\""), 5, "\"#.?\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "tiles[0]", "unknown tile '?'", "(2, 0)"})){
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
    database.add<Test_OverworldMaps>("field of the wrong type", test_bad_field_type);
    database.add<Test_OverworldMaps>("row of the wrong length", test_bad_row_length);
    database.add<Test_OverworldMaps>("unknown tile", test_unknown_tile);
    database.add<Test_OverworldMaps>("not JSON", test_not_json);
}



}
}
}
