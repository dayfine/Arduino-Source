/*  Map Data Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  parse_map() on a small synthetic map: the good map, a map with only the
 *  required fields, and one row per error class; and on a map shaped like
 *  each game's (BDSP, FRLG), for the fields only that game writes today.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <set>
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

//  A map shaped like BDSP's: on a world grid, with tiles off the game's
//  map, a region, a warp tile stood on, trainers, a person the story hides
//  and scene triggers.
const char* const BDSP_MAP = R"({
    "name": "Route 201",
    "source": {"decomp_file": "pret/pokediamond: land_data narc_0000.bin", "revision": "default branch", "date": "2026-11-01"},
    "world": {"name": "Sinnoh", "x": 96, "y": 839},
    "tiles": ["  ###", " #..#", "#.~..", "#..v#"],
    "points": {"save_spot": [2, 1]},
    "regions": {"lawn": [[3, 2], [1, 2], [1, 3]]},
    "warps": [{"x": 4, "y": 2, "enter": "step_on"}],
    "objects": [
        {"name": "youngster_tristan", "kind": "npc", "x": 3, "y": 1, "facing": "west", "trainer": {"sight": 2}},
        {"name": "lass_natalie", "kind": "npc", "x": 1, "y": 3, "facing": "north", "trainer": {"sight": 3, "looks": ["north", "east"]}},
        {"name": "rowan", "kind": "npc", "x": 2, "y": 3, "hide_flag": "story_after_00.03"}
    ],
    "triggers": [
        {"name": "barry_warning", "box": {"x0": 2, "y0": 1, "x1": 3, "y1": 1}, "when": "00.02"},
        {"name": "border_sign", "box": {"x0": 1, "y0": 2, "x1": 1, "y1": 2}}
    ]
})";

//  A map shaped like FRLG's: elevations, one-sided walls, warps of every
//  entry to per-warp points (one to a map not loaded), a person hidden by
//  an event flag, and a flag set.
const char* const FRLG_MAP = R"({
    "name": "CeruleanCave_1F",
    "source": {"decomp_file": "data/maps/CeruleanCave_1F/map.json", "revision": "ce6406d8c2ea", "date": "2026-11-01"},
    "width": 4,
    "tiles": ["#..#", "8.2#", "4.6."],
    "elevation": ["3303", "3F03", "0a33"],
    "points": {"warp_0": [1, 0]},
    "warps": [
        {"x": 1, "y": 0, "enter": "cave_entrance", "to_map": "CeruleanCity", "to_point": "warp_6"},
        {"x": 2, "y": 0, "enter": "step_on"},
        {"x": 1, "y": 1, "enter": "walk_north", "to_map": "CeruleanCave_2F", "to_point": "warp_0"},
        {"x": 2, "y": 1, "enter": "walk_south", "to_map": "CeruleanCave_2F", "to_point": "warp_1"},
        {"x": 1, "y": 2, "enter": "walk_west", "to_map": "CeruleanCave_2F", "to_point": "warp_2"},
        {"x": 3, "y": 2, "enter": "walk_east", "to_map": "CeruleanCave_2F", "to_point": "warp_3"}
    ],
    "objects": [
        {"name": "cooltrainer_m_1", "kind": "npc", "x": 1, "y": 1, "hide_flag": "FLAG_HIDE_CERULEAN_CAVE_GUARD",
         "wander": {"x0": 1, "y0": 1, "x1": 2, "y1": 1}}
    ],
    "flag_sets": {"after_hall_of_fame": ["FLAG_B", "FLAG_HIDE_CERULEAN_CAVE_GUARD", "FLAG_B"]}
})";

//  `base` with `find` replaced by `replacement`; empty when `find` is not
//  in it.
std::string edited(const char* base, const std::string& find, const std::string& replacement){
    std::string json = base;
    const size_t at = json.find(find);
    if (at == std::string::npos){
        return "";
    }
    return json.replace(at, find.size(), replacement);
}
std::string edited(const std::string& find, const std::string& replacement){
    return edited(VALID_MAP, find, replacement);
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
        || map.warps[0].enter != WarpEntry::WALK_INTO || !map.warps[0].to
        || map.warps[0].to->map != "TestHouse" || map.warps[0].to->point != "exit"
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
    if (map.tiles.width != 3 || map.tiles.height != 1 || map.world || map.elevation
        || !map.points.empty() || !map.regions.empty() || !map.warps.empty() || !map.objects.empty()
        || !map.triggers.empty() || !map.flag_sets.empty() || !map.connections.empty() || !map.patches.empty()
    ){
        return "Expected a 3x1 map with nothing on it.";
    }
    return true;
}

UnitTestResult test_glyphs_round_trip(){
    for (char glyph : std::string("#.~wv^<>DS 8246")){
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

//  The first row whose map doesn't fail as it says, or true.
UnitTestResult first_wrong_error(const std::vector<BadMap>& rows){
    for (const BadMap& row : rows){
        if (row.json.empty()){
            return std::string(row.name) + ": the text to edit is not in its map.";
        }
        const std::string error = parse_error(row.json);
        if (!mentions(error, row.mentions)){
            return std::string(row.name) + ": expected an error naming " + row.mentions.back() + ", got: \"" + error + "\"";
        }
    }
    return true;
}

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
        {.name = "warp missing to_point", .json = edited("\"to_point\": \"exit\"", "\"to\": \"exit\""), .mentions = {"warps[0].to_point is missing"}},
        {.name = "warp missing to_map", .json = edited("\"to_map\": \"TestHouse\"", "\"map\": \"TestHouse\""), .mentions = {"warps[0].to_map is missing"}},
        {.name = "warp entry unknown", .json = edited("\"to_map\": \"TestHouse\"", "\"enter\": \"jump\", \"to_map\": \"TestHouse\""), .mentions = {"warps[0].enter", "jump", "walk_into"}},
        {.name = "warp field of the wrong type", .json = edited("{\"x\": 1, \"y\": 2, \"to_map\"", "{\"x\": \"1\", \"y\": 2, \"to_map\""), .mentions = {"warps[0].x is not an integer"}},
        {.name = "object kind unknown", .json = edited("\"kind\": \"item\"", "\"kind\": \"boss\""), .mentions = {"objects[1].kind", "boss", "npc, item or sign"}},
        {.name = "object facing unknown", .json = edited("\"facing\": \"south\"", "\"facing\": \"down\""), .mentions = {"objects[0].facing", "down", "north, east, south or west"}},
        {.name = "wander not a box", .json = edited("\"x1\": 2, \"y1\": 2", "\"x1\": 0, \"y1\": 2"), .mentions = {"objects[0].wander is not a box"}},
        {.name = "wander outside", .json = edited("\"x1\": 2, \"y1\": 2", "\"x1\": 2, \"y1\": 7"), .mentions = {"objects[0].wander", "(2, 7)", "outside"}},
        {.name = "connection side unknown", .json = edited("\"side\": \"east\"", "\"side\": \"up\""), .mentions = {"connections[0].side", "up"}},
        {.name = "patch glyph unknown", .json = edited("\"glyph\": \"#\"", "\"glyph\": \"?\""), .mentions = {"patches[0].glyph", "?", "not a tile glyph"}},
        {.name = "patch outside", .json = edited("{\"x\": 2, \"y\": 1, \"glyph\"", "{\"x\": 2, \"y\": 9, \"glyph\""), .mentions = {"patches[0]", "(2, 9)", "outside"}},
    };
    return first_wrong_error(rows);
}

//  One value read from a parsed map, and what it should be, both as text.
struct Expected{
    const char* what;
    std::string expected;
    std::string read;
};

//  The first row whose value was read wrong, or true.
UnitTestResult first_wrong(const std::vector<Expected>& rows){
    for (const Expected& row : rows){
        if (row.read != row.expected){
            return std::format("{}: expected \"{}\", read \"{}\".", row.what, row.expected, row.read);
        }
    }
    return true;
}

std::string text(const std::optional<std::string>& value){
    return value ? *value : "(none)";
}
std::string text(const std::vector<TilePos>& tiles){
    std::string out;
    for (const TilePos& pos : tiles){
        out += to_string(pos);
    }
    return out;
}
std::string text(const std::optional<MapTrainer>& trainer){
    if (!trainer){
        return "(none)";
    }
    std::string out = std::format("sight {}, looks", trainer->sight);
    for (Direction direction : trainer->looks){
        out += std::string(" ") + direction_name(direction);
    }
    return out;
}
std::string text(const TileBox& box){
    return std::format("({}, {})..({}, {})", box.x0, box.y0, box.x1, box.y1);
}
std::string text(const MapWarp* warp){
    if (warp == nullptr){
        return "(none)";
    }
    const std::string to = warp->to ? warp->to->map + "." + warp->to->point : "(not loaded)";
    return std::format("{} {} -> {}", to_string(TilePos{warp->x, warp->y}), (int)warp->enter, to);
}
std::string text(const std::set<std::string>& flags){
    std::string out;
    for (const std::string& flag : flags){
        out += flag + " ";
    }
    return out;
}

UnitTestResult test_bdsp_shaped_map(){
    const MapData map = parse_map(parse_json(BDSP_MAP), "Route201.json");
    const std::string world = map.world ? map.world->name + " " + to_string(map.world->offset) : "(none)";
    return first_wrong({
        {.what = "world", .expected = "Sinnoh (96, 839)", .read = world},
        {.what = "tile (0, 0)", .expected = "' '", .read = std::format("'{}'", glyph_of(map.tiles.at({0, 0})))},
        {.what = "tile (0, 1) is off the map", .expected = "1", .read = std::to_string(map.tiles.at({0, 1}) == TileKind::OUTSIDE)},
        {.what = "region lawn, in order", .expected = "(3, 2)(1, 2)(1, 3)", .read = text(map.regions.at("lawn"))},
        {.what = "warp_at (4, 2)", .expected = std::format("(4, 2) {} -> (not loaded)", (int)WarpEntry::STEP_ON), .read = text(warp_at(map, {4, 2}))},
        {.what = "warp_at (3, 2)", .expected = "(none)", .read = text(warp_at(map, {3, 2}))},
        {.what = "Tristan, sight along his facing", .expected = "sight 2, looks west", .read = text(map.objects[0].trainer)},
        {.what = "Natalie, sight both ways", .expected = "sight 3, looks north east", .read = text(map.objects[1].trainer)},
        {.what = "Rowan is no trainer", .expected = "(none)", .read = text(map.objects[2].trainer)},
        {.what = "Rowan's hide flag", .expected = "story_after_00.03", .read = text(map.objects[2].hide_flag)},
        {.what = "Tristan's hide flag", .expected = "(none)", .read = text(map.objects[0].hide_flag)},
        {.what = "trigger 0", .expected = "barry_warning (2, 1)..(3, 1) 00.02", .read = std::format(
            "{} {} {}", map.triggers[0].name, text(map.triggers[0].box), text(map.triggers[0].when)
        )},
        {.what = "trigger 1, any time", .expected = "border_sign (none)", .read = map.triggers[1].name + " " + text(map.triggers[1].when)},
    });
}

UnitTestResult test_frlg_shaped_map(){
    const MapData map = parse_map(parse_json(FRLG_MAP), "CeruleanCave_1F.json");
    if (!map.elevation || map.elevation->width != 4 || map.elevation->height != 3){
        return "Expected 4x3 elevations.";
    }
    const auto tile = [&](const TilePos& pos){
        return std::format("'{}'", glyph_of(map.tiles.at(pos)));
    };
    const auto elevation = [&](const TilePos& pos){
        return std::to_string(map.elevation->at(pos));
    };
    const auto warp = [&](WarpEntry enter, const TilePos& pos, const char* to){
        return std::format("{} {} -> {}", to_string(pos), (int)enter, to);
    };
    return first_wrong({
        {.what = "closed north", .expected = "'8'", .read = tile({0, 1})},
        {.what = "closed south", .expected = "'2'", .read = tile({2, 1})},
        {.what = "closed west", .expected = "'4'", .read = tile({0, 2})},
        {.what = "closed east", .expected = "'6'", .read = tile({2, 2})},
        {.what = "elevation (2, 0)", .expected = "0", .read = elevation({2, 0})},
        {.what = "elevation (1, 1), upper case", .expected = "15", .read = elevation({1, 1})},
        {.what = "elevation (1, 2), lower case", .expected = "10", .read = elevation({1, 2})},
        {.what = "cave entrance", .expected = warp(WarpEntry::CAVE_ENTRANCE, {1, 0}, "CeruleanCity.warp_6"), .read = text(warp_at(map, {1, 0}))},
        {.what = "ladder to a map not loaded", .expected = warp(WarpEntry::STEP_ON, {2, 0}, "(not loaded)"), .read = text(warp_at(map, {2, 0}))},
        {.what = "mat north", .expected = warp(WarpEntry::WALK_NORTH, {1, 1}, "CeruleanCave_2F.warp_0"), .read = text(warp_at(map, {1, 1}))},
        {.what = "mat south", .expected = warp(WarpEntry::WALK_SOUTH, {2, 1}, "CeruleanCave_2F.warp_1"), .read = text(warp_at(map, {2, 1}))},
        {.what = "mat west", .expected = warp(WarpEntry::WALK_WEST, {1, 2}, "CeruleanCave_2F.warp_2"), .read = text(warp_at(map, {1, 2}))},
        {.what = "mat east", .expected = warp(WarpEntry::WALK_EAST, {3, 2}, "CeruleanCave_2F.warp_3"), .read = text(warp_at(map, {3, 2}))},
        {.what = "the guard's hide flag", .expected = "FLAG_HIDE_CERULEAN_CAVE_GUARD", .read = text(map.objects[0].hide_flag)},
        {.what = "flag set, each flag once", .expected = "FLAG_B FLAG_HIDE_CERULEAN_CAVE_GUARD ", .read = text(map.flag_sets.at("after_hall_of_fame"))},
    });
}

UnitTestResult test_each_error_class_of_game_fields(){
    return first_wrong_error({
        {.name = "world missing a field", .json = edited(BDSP_MAP, "\"name\": \"Sinnoh\"", "\"grid\": \"Sinnoh\""), .mentions = {"world.name is missing"}},
        {.name = "unknown glyph", .json = edited(BDSP_MAP, "\"  ###\"", "\"  #9#\""), .mentions = {"tiles[0]", "unknown glyph '9'"}},
        {.name = "region empty", .json = edited(BDSP_MAP, "[[3, 2], [1, 2], [1, 3]]", "[]"), .mentions = {"regions.lawn", "non-empty"}},
        {.name = "region tile not a pair", .json = edited(BDSP_MAP, "[[3, 2], [1, 2]", "[[3, 2], [1]"), .mentions = {"regions.lawn[1]", "[x, y]"}},
        {.name = "region tile outside", .json = edited(BDSP_MAP, "[[3, 2], [1, 2]", "[[3, 2], [9, 2]"), .mentions = {"regions.lawn[1]", "(9, 2)", "outside"}},
        {.name = "trainer with no way to look", .json = edited(BDSP_MAP, "\"facing\": \"west\", ", ""), .mentions = {"objects[0].trainer.looks is missing"}},
        {.name = "trainer sight below 1", .json = edited(BDSP_MAP, "\"sight\": 2", "\"sight\": 0"), .mentions = {"objects[0].trainer.sight 0", "at least 1"}},
        {.name = "trainer looks unknown", .json = edited(BDSP_MAP, "[\"north\", \"east\"]", "[\"north\", \"up\"]"), .mentions = {"objects[1].trainer.looks[1]", "up"}},
        {.name = "hide flag not a string", .json = edited(BDSP_MAP, "\"story_after_00.03\"", "3"), .mentions = {"objects[2].hide_flag is not a string"}},
        {.name = "trigger missing its box", .json = edited(BDSP_MAP, "\"box\": {\"x0\": 2", "\"area\": {\"x0\": 2"), .mentions = {"triggers[0].box is missing"}},
        {.name = "trigger outside", .json = edited(BDSP_MAP, "\"x1\": 3, \"y1\": 1", "\"x1\": 7, \"y1\": 1"), .mentions = {"triggers[0].box", "(7, 1)", "outside"}},
        {.name = "warp entry unknown", .json = edited(FRLG_MAP, "\"step_on\"", "\"fly\""), .mentions = {"warps[1].enter", "fly"}},
        {.name = "elevation rows missing", .json = edited(FRLG_MAP, ", \"0a33\"]", "]"), .mentions = {"elevation has 2 rows", "tiles' 3"}},
        {.name = "elevation row short", .json = edited(FRLG_MAP, "\"3F03\"", "\"3F0\""), .mentions = {"elevation[1]", "4 hex digits"}},
        {.name = "elevation not hex", .json = edited(FRLG_MAP, "\"3F03\"", "\"3G03\""), .mentions = {"elevation[1]", "'G'", "not a hex digit"}},
        {.name = "flag set not a list", .json = edited(FRLG_MAP, "[\"FLAG_B\", \"FLAG_HIDE_CERULEAN_CAVE_GUARD\", \"FLAG_B\"]", "\"FLAG_B\""), .mentions = {"flag_sets.after_hall_of_fame", "not a list"}},
        {.name = "flag not a string", .json = edited(FRLG_MAP, "[\"FLAG_B\", ", "[7, "), .mentions = {"flag_sets.after_hall_of_fame[0] is not a string"}},
    });
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
    database.add<Test_MapData>("a map shaped like BDSP's", test_bdsp_shaped_map);
    database.add<Test_MapData>("a map shaped like FRLG's", test_frlg_shaped_map);
    database.add<Test_MapData>("each error class of the game fields", test_each_error_class_of_game_fields);
    database.add<Test_MapData>("maps built into the program", test_built_in_maps);
}



}
}
