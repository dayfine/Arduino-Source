/*  Map Data Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  parse_map() on a small synthetic map: the good map, a map with only the
 *  required fields, and one row per error class.
 *
 */

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_MapData.h"
#include "Pokemon_Navigation_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

const char* const VALID_MAP = R"({
    "name": "TestTown",
    "source": {"decomp_file": "data/maps/TestTown/map.json", "revision": "abc123", "date": "2026-10-31"},
    "tiles": ["####", "#..~", "#D.S", "w^v#"],
    "points": {"door": [1, 2], "sign": [3, 2]},
    "warps": [{"x": 1, "y": 2, "to_map": "TestHouse", "to_point": "exit"}],
    "objects": [
        {"name": "mom", "kind": "npc", "x": 2, "y": 1, "facing": "south", "wander": {"x0": 1, "y0": 1, "x1": 2, "y1": 2}},
        {"name": "potion", "kind": "item", "x": 2, "y": 2}
    ],
    "connections": [{"side": "east", "to_map": "Route 1", "offset": 0}],
    "patches": [{"x": 2, "y": 1, "glyph": "#", "measured_by": "run 3, 2026-10-30"}]
})";

//  The valid map with `find` replaced by `replacement`; empty when `find`
//  is not in it.
std::string edited(const std::string& find, const std::string& replacement){
    std::string json = VALID_MAP;
    const size_t at = json.find(find);
    if (at == std::string::npos){
        return "";
    }
    return json.replace(at, find.size(), replacement);
}

//  The ParseException that parsing `json` throws, or empty if it doesn't.
std::string parse_error(const std::string& json){
    try{
        parse_map(parse_json(json), "Test.json");
    }catch (ParseException& e){
        return e.message();
    }
    return "";
}

//  Whether `text` contains every one of `parts`.
bool mentions(const std::string& text, const std::vector<const char*>& parts){
    for (const char* part : parts){
        if (text.find(part) == std::string::npos){
            return false;
        }
    }
    return true;
}


UnitTestResult test_valid_map(){
    const MapData map = parse_map(parse_json(VALID_MAP), "Test.json");
    if (map.name != "TestTown" || map.source.decomp_file != "data/maps/TestTown/map.json"
        || map.source.revision != "abc123" || map.source.date != "2026-10-31"
    ){
        return "Wrong name or source.";
    }
    if (map.tiles.width != 4 || map.tiles.height != 4
        || map.tiles.at({3, 1}) != TileKind::TALL_GRASS || map.tiles.at({1, 2}) != TileKind::DOOR
        || map.tiles.at({3, 2}) != TileKind::SIGN || map.tiles.at({0, 3}) != TileKind::WATER
        || map.tiles.at({1, 3}) != TileKind::LEDGE_NORTH || map.tiles.at({2, 3}) != TileKind::LEDGE_SOUTH
    ){
        return "Wrong tiles.";
    }
    if (map.tiles.at({2, 1}) != TileKind::WALL || map.patches.size() != 1 || map.patches[0].measured_by != "run 3, 2026-10-30"){
        return "The patch should have made (2, 1) a wall.";
    }
    if (map.points.size() != 2 || map.points.at("door") != TilePos{1, 2} || map.points.at("sign") != TilePos{3, 2}){
        return "Wrong points.";
    }
    if (map.warps.size() != 1 || map.warps[0].x != 1 || map.warps[0].y != 2
        || map.warps[0].to_map != "TestHouse" || map.warps[0].to_point != "exit"
    ){
        return "Wrong warps.";
    }
    if (map.objects.size() != 2 || map.objects[0].name != "mom" || map.objects[0].kind != ObjectKind::NPC
        || map.objects[0].facing != Direction::SOUTH || !map.objects[0].wander
        || map.objects[0].wander->x0 != 1 || map.objects[0].wander->y1 != 2
        || map.objects[1].kind != ObjectKind::ITEM || map.objects[1].facing || map.objects[1].wander
    ){
        return "Wrong objects.";
    }
    if (map.connections.size() != 1 || map.connections[0].side != Direction::EAST
        || map.connections[0].to_map != "Route 1" || map.connections[0].offset != 0
    ){
        return "Wrong connections.";
    }
    return true;
}

UnitTestResult test_lists_may_be_left_out(){
    const MapData map = parse_map(parse_json(R"({
        "name": "Cave",
        "source": {"decomp_file": "f", "revision": "r", "date": "d"},
        "tiles": ["#.#"]
    })"), "Cave.json");
    if (map.tiles.width != 3 || map.tiles.height != 1 || !map.points.empty() || !map.warps.empty()
        || !map.objects.empty() || !map.connections.empty() || !map.patches.empty()
    ){
        return "Expected a 3x1 map with nothing on it.";
    }
    return true;
}

UnitTestResult test_glyphs_round_trip(){
    for (char glyph : std::string("#.~wv^<>DS")){
        const std::optional<TileKind> tile = tile_from_glyph(glyph);
        if (!tile || glyph_of(*tile) != glyph){
            return std::string("Glyph '") + glyph + "' does not round-trip.";
        }
    }
    if (tile_from_glyph('?')){
        return "'?' is not a glyph.";
    }
    return true;
}

struct BadMap{
    const char* name;
    std::string json;
    std::vector<const char*> mentions;
};

UnitTestResult test_each_error_class(){
    const std::vector<BadMap> rows = {
        {.name = "not an object", .json = "[]", .mentions = {"Test.json", "not a JSON object"}},
        {.name = "missing name", .json = edited("\"name\": \"TestTown\"", "\"title\": \"TestTown\""), .mentions = {"Test.json", "name is missing"}},
        {.name = "name not a string", .json = edited("\"name\": \"TestTown\"", "\"name\": 7"), .mentions = {"name is not a string"}},
        {.name = "source missing a field", .json = edited("\"revision\": \"abc123\"", "\"rev\": \"abc123\""), .mentions = {"source.revision is missing"}},
        {.name = "tiles without rows", .json = edited("\"tiles\": [\"####\", \"#..~\", \"#D.S\", \"w^v#\"]", "\"tiles\": []"), .mentions = {"tiles has no rows"}},
        {.name = "ragged row", .json = edited("\"#..~\"", "\"#..~.\""), .mentions = {"tiles[1]", "string of 4 glyphs"}},
        {.name = "unknown glyph", .json = edited("\"#..~\"", "\"#.?~\""), .mentions = {"tiles[1]", "unknown glyph '?'", "(2, 1)"}},
        {.name = "point not a pair", .json = edited("\"door\": [1, 2]", "\"door\": [1]"), .mentions = {"points.door", "[x, y]"}},
        {.name = "point outside", .json = edited("\"sign\": [3, 2]", "\"sign\": [4, 2]"), .mentions = {"points.sign", "(4, 2)", "outside the 4x4 map"}},
        {.name = "warp missing a field", .json = edited("\"to_point\": \"exit\"", "\"to\": \"exit\""), .mentions = {"warps[0].to_point is missing"}},
        {.name = "warp field of the wrong type", .json = edited("{\"x\": 1, \"y\": 2, \"to_map\"", "{\"x\": \"1\", \"y\": 2, \"to_map\""), .mentions = {"warps[0].x is not an integer"}},
        {.name = "object kind unknown", .json = edited("\"kind\": \"item\"", "\"kind\": \"boss\""), .mentions = {"objects[1].kind", "boss", "npc, item or sign"}},
        {.name = "object facing unknown", .json = edited("\"facing\": \"south\"", "\"facing\": \"down\""), .mentions = {"objects[0].facing", "down", "north, east, south or west"}},
        {.name = "wander not a box", .json = edited("\"x1\": 2, \"y1\": 2", "\"x1\": 0, \"y1\": 2"), .mentions = {"objects[0].wander is not a box"}},
        {.name = "wander outside", .json = edited("\"x1\": 2, \"y1\": 2", "\"x1\": 2, \"y1\": 7"), .mentions = {"objects[0].wander", "(2, 7)", "outside"}},
        {.name = "connection side unknown", .json = edited("\"side\": \"east\"", "\"side\": \"up\""), .mentions = {"connections[0].side", "up"}},
        {.name = "patch glyph unknown", .json = edited("\"glyph\": \"#\"", "\"glyph\": \"?\""), .mentions = {"patches[0].glyph", "?", "not a tile glyph"}},
        {.name = "patch outside", .json = edited("{\"x\": 2, \"y\": 1, \"glyph\"", "{\"x\": 2, \"y\": 9, \"glyph\""), .mentions = {"patches[0]", "(2, 9)", "outside"}},
    };
    for (const BadMap& row : rows){
        if (row.json.empty()){
            return std::string(row.name) + ": the text to edit is not in the valid map.";
        }
        const std::string error = parse_error(row.json);
        if (!mentions(error, row.mentions)){
            return std::string(row.name) + ": expected an error naming " + row.mentions.back() + ", got: \"" + error + "\"";
        }
    }
    return true;
}


class Test_MapData : public UnitTest{
public:
    Test_MapData(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("Pokemon::MapData - " + std::move(name))
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

//  The maps in TestMaps/, read back from the program: the small one, and one
//  longer than a string literal may be, which the build splits.
UnitTestResult test_built_in_maps(){
    const std::vector<std::pair<std::string, std::string>>& files = navigation_test_map_json();
    if (files.size() != 2 || files[0].first != "BigField" || files[1].first != "Cave"){
        return "Expected BigField and Cave, sorted by name.";
    }
    const MapData big = parse_map(parse_json(files[0].second), "BigField.json");
    if (files[0].second.size() <= 16000 || big.tiles.width != 130 || big.tiles.height != 130){
        return "Expected BigField's 130x130 tiles, over 16000 bytes of text.";
    }
    for (int y = 0; y < 130; y++){
        for (int x = 0; x < 130; x++){
            const bool border = x == 0 || y == 0 || x == 129 || y == 129;
            if (big.tiles.at(TilePos{x, y}) != (border ? TileKind::WALL : TileKind::OPEN)){
                return "BigField's tile (" + std::to_string(x) + ", " + std::to_string(y) + ") is wrong.";
            }
        }
    }
    const MapData cave = parse_map(parse_json(files[1].second), "Cave.json");
    if (cave.tiles.width != 3 || cave.tiles.height != 1){
        return "Expected Cave's 3x1 tiles.";
    }
    return true;
}

}


void add_tests_MapData(UnitTestDatabase& database){
    database.add<Test_MapData>("the valid map", test_valid_map);
    database.add<Test_MapData>("lists may be left out", test_lists_may_be_left_out);
    database.add<Test_MapData>("glyphs round-trip", test_glyphs_round_trip);
    database.add<Test_MapData>("each error class", test_each_error_class);
    database.add<Test_MapData>("maps built into the program", test_built_in_maps);
}



}
}
