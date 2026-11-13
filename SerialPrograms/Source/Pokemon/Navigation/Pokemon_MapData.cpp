/*  Map Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <format>
#include <string_view>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon_RuntimeMap.h"
#include "Pokemon_MapData.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

constexpr std::pair<char, TileKind> GLYPHS[] = {
    {'#', TileKind::WALL},
    {'.', TileKind::OPEN},
    {'~', TileKind::TALL_GRASS},
    {'w', TileKind::WATER},
    {'^', TileKind::LEDGE_NORTH},
    {'v', TileKind::LEDGE_SOUTH},
    {'<', TileKind::LEDGE_WEST},
    {'>', TileKind::LEDGE_EAST},
    {'D', TileKind::DOOR},
    {'S', TileKind::SIGN},
    {' ', TileKind::OUTSIDE},
    {'8', TileKind::CLOSED_NORTH},
    {'2', TileKind::CLOSED_SOUTH},
    {'4', TileKind::CLOSED_WEST},
    {'6', TileKind::CLOSED_EAST},
    {'G', TileKind::GATE},
};

constexpr std::pair<std::string_view, WarpEntry> WARP_ENTRIES[] = {
    {"walk_into", WarpEntry::WALK_INTO},
    {"step_on", WarpEntry::STEP_ON},
    {"cave_entrance", WarpEntry::CAVE_ENTRANCE},
    {"walk_north", WarpEntry::WALK_NORTH},
    {"walk_south", WarpEntry::WALK_SOUTH},
    {"walk_west", WarpEntry::WALK_WEST},
    {"walk_east", WarpEntry::WALK_EAST},
};

} // namespace

std::optional<TileKind> tile_from_glyph(char glyph){
    for (const auto& [known, tile] : GLYPHS){
        if (known == glyph){
            return tile;
        }
    }
    return std::nullopt;
}

char glyph_of(TileKind tile){
    for (const auto& [glyph, known] : GLYPHS){
        if (known == tile){
            return glyph;
        }
    }
    return '?';
}


namespace{

//  The readers take `where`, the source and the path to the object being
//  read (e.g. "Maps/TestTown.json: warps[2]."), and name the field in what
//  they throw.

const JsonObject& as_object(const JsonValue& value, const std::string& where){
    const JsonObject* object = value.to_object();
    if (object == nullptr){
        throw ParseException(std::format("{} is not a JSON object.", where));
    }
    return *object;
}

const JsonValue& read_field(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue* value = object.get_value(key);
    if (value == nullptr){
        throw ParseException(std::format("{}{} is missing.", where, key));
    }
    return *value;
}

int read_int(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue& value = read_field(object, key, where);
    if (!value.is_integer()){
        throw ParseException(std::format("{}{} is not an integer.", where, key));
    }
    return (int)value.to_integer_default();
}

std::string read_string(const JsonObject& object, const std::string& key, const std::string& where){
    const std::string* text = read_field(object, key, where).to_string();
    if (text == nullptr){
        throw ParseException(std::format("{}{} is not a string.", where, key));
    }
    return *text;
}

//  The boolean `key`, or `fallback` when the map leaves it out.
bool optional_bool(const JsonObject& object, const std::string& key, bool fallback, const std::string& where){
    const JsonValue* value = object.get_value(key);
    if (value == nullptr){
        return fallback;
    }
    if (!value->is_boolean()){
        throw ParseException(std::format("{}{} is not true or false.", where, key));
    }
    return value->to_boolean_default(fallback);
}

//  The string `key`, or nothing when the map leaves it out.
std::optional<std::string> optional_string(const JsonObject& object, const std::string& key, const std::string& where){
    if (object.get_value(key) == nullptr){
        return std::nullopt;
    }
    return read_string(object, key, where);
}

TilePos read_pos(const JsonObject& object, const std::string& where){
    return TilePos{read_int(object, "x", where), read_int(object, "y", where)};
}

Direction parse_direction(const std::string& name, const std::string& where){
    for (Direction candidate : ALL_DIRECTIONS){
        if (name == direction_name(candidate)){
            return candidate;
        }
    }
    throw ParseException(std::format("{} \"{}\" is not north, east, south or west.", where, name));
}

Direction read_direction(const JsonObject& object, const std::string& key, const std::string& where){
    return parse_direction(read_string(object, key, where), std::format("{}{}", where, key));
}

//  The list `key`, or nothing when the map leaves it out.
const JsonArray* optional_array(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue* value = object.get_value(key);
    if (value == nullptr){
        return nullptr;
    }
    const JsonArray* array = value->to_array();
    if (array == nullptr){
        throw ParseException(std::format("{}{} is not an array.", where, key));
    }
    return array;
}

const JsonObject* optional_object(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue* value = object.get_value(key);
    if (value == nullptr){
        return nullptr;
    }
    return &as_object(*value, std::format("{}{}", where, key));
}

template <typename Item>
std::vector<Item> read_list(
    const JsonObject& object, const std::string& key, const std::string& where,
    Item (*read_item)(const JsonObject& object, const std::string& where)
){
    std::vector<Item> items;
    const JsonArray* array = optional_array(object, key, where);
    if (array == nullptr){
        return items;
    }
    for (size_t c = 0; c < array->size(); c++){
        const std::string item_where = std::format("{}{}[{}]", where, key, c);
        items.push_back(read_item(as_object((*array)[c], item_where), std::format("{}.", item_where)));
    }
    return items;
}


MapSource read_source(const JsonObject& root, const std::string& where){
    //  Named, since GCC warns of a reference returned for a temporary path.
    const std::string key = "source";
    const std::string path = std::format("{}{}", where, key);
    const JsonObject& object = as_object(read_field(root, key, where), path);
    const std::string source_where = std::format("{}.", path);
    MapSource source;
    source.decomp_file = read_string(object, "decomp_file", source_where);
    source.revision = read_string(object, "revision", source_where);
    source.date = read_string(object, "date", source_where);
    return source;
}

std::optional<MapWorld> read_world(const JsonObject& root, const std::string& where){
    const JsonObject* object = optional_object(root, "world", where);
    if (object == nullptr){
        return std::nullopt;
    }
    const std::string world_where = std::format("{}world.", where);
    return MapWorld{
        .name = read_string(*object, "name", world_where),
        .offset = read_pos(*object, world_where),
    };
}

//  Rows of glyphs, all as wide as the first.
Grid<TileKind> read_tiles(const JsonObject& root, const std::string& where){
    const std::string key = "tiles";
    const JsonValue& value = read_field(root, key, where);
    const JsonArray* rows = value.to_array();
    if (rows == nullptr){
        throw ParseException(std::format("{}{} is not an array.", where, key));
    }
    if (rows->empty()){
        throw ParseException(std::format("{}{} has no rows.", where, key));
    }
    const std::string* first = (*rows)[0].to_string();
    const size_t width = first == nullptr ? 0 : first->size();
    if (width == 0){
        throw ParseException(std::format("{}{}[0] is not a string of glyphs.", where, key));
    }
    Grid<TileKind> grid{(int)width, (int)rows->size(), {}};
    for (size_t y = 0; y < rows->size(); y++){
        const std::string row_where = std::format("{}{}[{}]", where, key, y);
        const std::string* row = (*rows)[y].to_string();
        if (row == nullptr || row->size() != width){
            throw ParseException(std::format("{} is not a string of {} glyphs.", row_where, width));
        }
        for (size_t x = 0; x < width; x++){
            const std::optional<TileKind> tile = tile_from_glyph((*row)[x]);
            if (!tile){
                throw ParseException(std::format(
                    "{} has unknown glyph '{}' at {}.", row_where, (*row)[x], to_string(TilePos{(int)x, (int)y})
                ));
            }
            grid.cells.push_back(*tile);
        }
    }
    return grid;
}

TilePos read_pair(const JsonValue& value, const std::string& where){
    const JsonArray* pair = value.to_array();
    if (pair == nullptr || pair->size() != 2 || !(*pair)[0].is_integer() || !(*pair)[1].is_integer()){
        throw ParseException(std::format("{} is not [x, y].", where));
    }
    return TilePos{(int)(*pair)[0].to_integer_default(), (int)(*pair)[1].to_integer_default()};
}

//  Rows of hex digits, one per tile.
std::optional<Grid<uint8_t>> read_elevation(const JsonObject& root, const Grid<TileKind>& tiles, const std::string& where){
    const std::string key = "elevation";
    const JsonArray* rows = optional_array(root, key, where);
    if (rows == nullptr){
        return std::nullopt;
    }
    if ((int)rows->size() != tiles.height){
        throw ParseException(std::format("{}{} has {} rows, not the tiles' {}.", where, key, rows->size(), tiles.height));
    }
    Grid<uint8_t> grid{tiles.width, tiles.height, {}};
    for (size_t y = 0; y < rows->size(); y++){
        const std::string row_where = std::format("{}{}[{}]", where, key, y);
        const std::string* row = (*rows)[y].to_string();
        if (row == nullptr || (int)row->size() != tiles.width){
            throw ParseException(std::format("{} is not a string of {} hex digits.", row_where, tiles.width));
        }
        for (size_t x = 0; x < row->size(); x++){
            const char digit = (*row)[x];
            const size_t value = std::string_view("0123456789abcdef").find((char)std::tolower((unsigned char)digit));
            if (value == std::string_view::npos){
                throw ParseException(std::format("{} has '{}' at {}, not a hex digit.", row_where, digit, to_string(TilePos{(int)x, (int)y})));
            }
            grid.cells.push_back((uint8_t)value);
        }
    }
    return grid;
}

//  Each flag set is a list of flag names.
std::map<std::string, std::set<std::string>> read_flag_sets(const JsonObject& root, const std::string& where){
    std::map<std::string, std::set<std::string>> sets;
    const JsonObject* object = optional_object(root, "flag_sets", where);
    if (object == nullptr){
        return sets;
    }
    for (const auto& [name, value] : *object){
        const std::string set_where = std::format("{}flag_sets.{}", where, name);
        const JsonArray* flags = value.to_array();
        if (flags == nullptr){
            throw ParseException(std::format("{} is not a list of flags.", set_where));
        }
        std::set<std::string>& set = sets[name];
        for (size_t c = 0; c < flags->size(); c++){
            const std::string* flag = (*flags)[c].to_string();
            if (flag == nullptr){
                throw ParseException(std::format("{}[{}] is not a string.", set_where, c));
            }
            set.insert(*flag);
        }
    }
    return sets;
}

std::map<std::string, TilePos> read_points(const JsonObject& root, const std::string& where){
    std::map<std::string, TilePos> points;
    const JsonObject* object = optional_object(root, "points", where);
    if (object == nullptr){
        return points;
    }
    for (const auto& [name, value] : *object){
        points[name] = read_pair(value, std::format("{}points.{}", where, name));
    }
    return points;
}

//  Each region is a non-empty list of [x, y].
std::map<std::string, std::vector<TilePos>> read_regions(const JsonObject& root, const std::string& where){
    std::map<std::string, std::vector<TilePos>> regions;
    const JsonObject* object = optional_object(root, "regions", where);
    if (object == nullptr){
        return regions;
    }
    for (const auto& [name, value] : *object){
        const std::string region_where = std::format("{}regions.{}", where, name);
        const JsonArray* tiles = value.to_array();
        if (tiles == nullptr || tiles->empty()){
            throw ParseException(std::format("{} is not a non-empty list of [x, y].", region_where));
        }
        std::vector<TilePos>& region = regions[name];
        for (size_t c = 0; c < tiles->size(); c++){
            region.push_back(read_pair((*tiles)[c], std::format("{}[{}]", region_where, c)));
        }
    }
    return regions;
}

WarpEntry read_warp_entry(const JsonObject& object, const std::string& where){
    const std::optional<std::string> name = optional_string(object, "enter", where);
    if (!name){
        return WarpEntry::WALK_INTO;
    }
    for (const auto& [known, entry] : WARP_ENTRIES){
        if (*name == known){
            return entry;
        }
    }
    throw ParseException(std::format(
        "{}enter \"{}\" is not walk_into, step_on, cave_entrance or walk_<direction>.", where, *name
    ));
}

//  `to_map` and `to_point` come together, or neither does.
MapWarp read_warp(const JsonObject& object, const std::string& where){
    MapWarp warp;
    const TilePos pos = read_pos(object, where);
    warp.x = pos.x;
    warp.y = pos.y;
    warp.enter = read_warp_entry(object, where);
    warp.one_way = optional_bool(object, "one_way", false, where);
    const bool has_map = object.get_value("to_map") != nullptr;
    const bool has_point = object.get_value("to_point") != nullptr;
    if (has_map != has_point){
        throw ParseException(std::format("{}{} is missing.", where, has_map ? "to_point" : "to_map"));
    }
    if (has_map){
        warp.to = WarpDestination{
            .map = read_string(object, "to_map", where),
            .point = read_string(object, "to_point", where),
        };
    }
    return warp;
}

ObjectKind read_object_kind(const JsonObject& object, const std::string& where){
    const std::string kind = read_string(object, "kind", where);
    if (kind == "npc"){
        return ObjectKind::NPC;
    }
    if (kind == "item"){
        return ObjectKind::ITEM;
    }
    if (kind == "sign"){
        return ObjectKind::SIGN;
    }
    throw ParseException(std::format("{}kind \"{}\" is not npc, item or sign.", where, kind));
}

TileBox read_box(const JsonObject& object, const std::string& where){
    TileBox box;
    box.x0 = read_int(object, "x0", where);
    box.y0 = read_int(object, "y0", where);
    box.x1 = read_int(object, "x1", where);
    box.y1 = read_int(object, "y1", where);
    if (box.x0 > box.x1 || box.y0 > box.y1){
        throw ParseException(std::format("{} is not a box (x0 <= x1, y0 <= y1).", where.substr(0, where.size() - 1)));
    }
    return box;
}

//  `looks` defaults to the trainer's facing.
MapTrainer read_trainer(const JsonObject& object, const std::optional<Direction>& facing, const std::string& where){
    MapTrainer trainer;
    trainer.sight = read_int(object, "sight", where);
    if (trainer.sight < 1){
        throw ParseException(std::format("{}sight {} is not at least 1.", where, trainer.sight));
    }
    const JsonArray* looks = optional_array(object, "looks", where);
    if (looks == nullptr){
        if (!facing){
            throw ParseException(std::format("{}looks is missing, and the object has no facing.", where));
        }
        trainer.looks.push_back(*facing);
        return trainer;
    }
    for (size_t c = 0; c < looks->size(); c++){
        const std::string item_where = std::format("{}looks[{}]", where, c);
        const std::string* name = (*looks)[c].to_string();
        if (name == nullptr){
            throw ParseException(std::format("{} is not a string.", item_where));
        }
        trainer.looks.push_back(parse_direction(*name, item_where));
    }
    return trainer;
}

MapObject read_object(const JsonObject& object, const std::string& where){
    MapObject map_object;
    map_object.name = read_string(object, "name", where);
    map_object.kind = read_object_kind(object, where);
    const TilePos pos = read_pos(object, where);
    map_object.x = pos.x;
    map_object.y = pos.y;
    if (object.get_value("facing") != nullptr){
        map_object.facing = read_direction(object, "facing", where);
    }
    if (const JsonObject* wander = optional_object(object, "wander", where)){
        map_object.wander = read_box(*wander, std::format("{}wander.", where));
    }
    if (const JsonObject* trainer = optional_object(object, "trainer", where)){
        map_object.trainer = read_trainer(*trainer, map_object.facing, std::format("{}trainer.", where));
    }
    map_object.hide_flag = optional_string(object, "hide_flag", where);
    return map_object;
}

MapTrigger read_trigger(const JsonObject& object, const std::string& where){
    MapTrigger trigger;
    trigger.name = read_string(object, "name", where);
    //  Named, since GCC warns of a reference returned for a temporary path.
    const std::string key = "box";
    trigger.box = read_box(as_object(read_field(object, key, where), std::format("{}{}", where, key)), std::format("{}{}.", where, key));
    trigger.when = optional_string(object, "when", where);
    return trigger;
}

MapConnection read_connection(const JsonObject& object, const std::string& where){
    MapConnection connection;
    connection.side = read_direction(object, "side", where);
    connection.to_map = read_string(object, "to_map", where);
    connection.offset = read_int(object, "offset", where);
    return connection;
}

MapPatch read_patch(const JsonObject& object, const std::string& where){
    MapPatch patch;
    const TilePos pos = read_pos(object, where);
    patch.x = pos.x;
    patch.y = pos.y;
    const std::string glyph = read_string(object, "glyph", where);
    const std::optional<TileKind> tile = glyph.size() == 1 ? tile_from_glyph(glyph[0]) : std::nullopt;
    if (!tile){
        throw ParseException(std::format("{}glyph \"{}\" is not a tile glyph.", where, glyph));
    }
    patch.glyph = *tile;
    patch.measured_by = read_string(object, "measured_by", where);
    return patch;
}

//  The tiles of a gate or a coordinate pair, `[x, y]` each.
std::vector<TilePos> read_pair_list(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonArray* array = optional_array(object, key, where);
    if (array == nullptr || array->empty()){
        throw ParseException(std::format("{}{} is not a non-empty list of [x, y].", where, key));
    }
    std::vector<TilePos> pairs;
    for (size_t c = 0; c < array->size(); c++){
        pairs.push_back(read_pair((*array)[c], std::format("{}{}[{}]", where, key, c)));
    }
    return pairs;
}

TilePos read_pair_field(const JsonObject& object, const std::string& key, const std::string& where){
    return read_pair(read_field(object, key, where), std::format("{}{}", where, key));
}

bool read_on_off(const JsonObject& object, const std::string& key, const std::string& where){
    const std::string text = read_string(object, key, where);
    if (text != "on" && text != "off"){
        throw ParseException(std::format("{}{} \"{}\" is not on or off.", where, key, text));
    }
    return text == "on";
}

//  A gate is a card-key door (opens_by and flag) or follows a switch (switch
//  and open_when); never both.
MapGate read_gate(const std::string& id, const JsonObject& object, const std::string& where){
    MapGate gate;
    gate.id = id;
    gate.tiles = read_pair_list(object, "tiles", where);
    const std::string glyph = read_string(object, "open_glyph", where);
    const std::optional<TileKind> open = glyph.size() == 1 ? tile_from_glyph(glyph[0]) : std::nullopt;
    if (!open || *open == TileKind::GATE){
        throw ParseException(std::format("{}open_glyph \"{}\" is not a tile glyph other than G.", where, glyph));
    }
    gate.open_tile = *open;
    const std::string new_game = read_string(object, "new_game", where);
    if (new_game != "open" && new_game != "closed"){
        throw ParseException(std::format("{}new_game \"{}\" is not open or closed.", where, new_game));
    }
    gate.open_at_new_game = new_game == "open";
    const bool by_key = object.get_value("opens_by") != nullptr || object.get_value("flag") != nullptr;
    const bool by_switch = object.get_value("switch") != nullptr || object.get_value("open_when") != nullptr;
    if (by_key == by_switch){
        throw ParseException(std::format("{}needs opens_by and flag, or switch and open_when, and not both.", where));
    }
    if (by_key){
        const std::string opens_by = read_string(object, "opens_by", where);
        if (opens_by != "card_key_face_a"){
            throw ParseException(std::format("{}opens_by \"{}\" is not card_key_face_a.", where, opens_by));
        }
        gate.kind = GateKind::CARD_KEY;
        gate.flag = read_string(object, "flag", where);
    }else{
        gate.kind = GateKind::SWITCH;
        gate.switch_id = read_string(object, "switch", where);
        gate.open_when_on = read_on_off(object, "open_when", where);
    }
    return gate;
}

std::map<std::string, MapGate> read_gates(const JsonObject& root, const std::string& where){
    std::map<std::string, MapGate> gates;
    const JsonObject* object = optional_object(root, "gates", where);
    if (object == nullptr){
        return gates;
    }
    for (const auto& [id, value] : *object){
        const std::string gate_where = std::format("{}gates.{}", where, id);
        gates[id] = read_gate(id, as_object(value, gate_where), gate_where + ".");
    }
    return gates;
}

MapStatue read_statue(const JsonObject& object, const std::string& where){
    MapStatue statue;
    statue.tile = read_pair_field(object, "coord", where);
    statue.face_from = read_pair_field(object, "face_from", where);
    statue.facing = read_direction(object, "facing", where);
    return statue;
}

TilePos read_button(const JsonObject& object, const std::string& where){
    return read_pair_field(object, "coord", where);
}

MapSwitch read_switch(const std::string& id, const JsonObject& object, const std::string& where){
    MapSwitch result;
    result.id = id;
    result.flag = read_string(object, "flag", where);
    const JsonValue* toggle = object.get_value("toggle");
    if (toggle != nullptr && !toggle->is_boolean()){
        throw ParseException(std::format("{}toggle is not true or false.", where));
    }
    result.toggle = toggle == nullptr || toggle->to_boolean_default(true);
    const JsonArray* floors = optional_array(object, "floors", where);
    if (floors == nullptr || floors->empty()){
        throw ParseException(std::format("{}floors is not a non-empty list of map names.", where));
    }
    for (size_t c = 0; c < floors->size(); c++){
        const std::string* floor = (*floors)[c].to_string();
        if (floor == nullptr){
            throw ParseException(std::format("{}floors[{}] is not a string.", where, c));
        }
        result.floors.push_back(*floor);
    }
    result.statues = read_list<MapStatue>(object, "statues", where, read_statue);
    result.buttons = read_list<TilePos>(object, "buttons", where, read_button);
    return result;
}

std::map<std::string, MapSwitch> read_switches(const JsonObject& root, const std::string& where){
    std::map<std::string, MapSwitch> switches;
    const JsonObject* object = optional_object(root, "switches", where);
    if (object == nullptr){
        return switches;
    }
    for (const auto& [id, value] : *object){
        const std::string switch_where = std::format("{}switches.{}", where, id);
        switches[id] = read_switch(id, as_object(value, switch_where), switch_where + ".");
    }
    return switches;
}


void check_inside(const Grid<TileKind>& tiles, const TilePos& pos, const std::string& where){
    if (!tiles.contains(pos)){
        throw ParseException(std::format("{} {} is outside the {}x{} map.", where, to_string(pos), tiles.width, tiles.height));
    }
}

void check_box_inside(const Grid<TileKind>& tiles, const TileBox& box, const std::string& where){
    check_inside(tiles, TilePos{box.x0, box.y0}, where);
    check_inside(tiles, TilePos{box.x1, box.y1}, where);
}

//  Everything with a place on the map has it inside the tiles.
void check_places(const MapData& map, const std::string& where){
    for (const auto& [name, pos] : map.points){
        check_inside(map.tiles, pos, std::format("{}points.{}", where, name));
    }
    for (const auto& [name, region] : map.regions){
        for (size_t c = 0; c < region.size(); c++){
            check_inside(map.tiles, region[c], std::format("{}regions.{}[{}]", where, name, c));
        }
    }
    for (size_t c = 0; c < map.warps.size(); c++){
        check_inside(map.tiles, TilePos{map.warps[c].x, map.warps[c].y}, std::format("{}warps[{}]", where, c));
    }
    for (size_t c = 0; c < map.objects.size(); c++){
        const MapObject& object = map.objects[c];
        check_inside(map.tiles, TilePos{object.x, object.y}, std::format("{}objects[{}]", where, c));
        if (object.wander){
            check_box_inside(map.tiles, *object.wander, std::format("{}objects[{}].wander", where, c));
        }
    }
    for (size_t c = 0; c < map.triggers.size(); c++){
        check_box_inside(map.tiles, map.triggers[c].box, std::format("{}triggers[{}].box", where, c));
    }
    for (size_t c = 0; c < map.patches.size(); c++){
        check_inside(map.tiles, TilePos{map.patches[c].x, map.patches[c].y}, std::format("{}patches[{}]", where, c));
    }
}

//  Every gate tile is a G tile, in one gate only; every G tile is in a
//  gate; a gate follows a switch the map lists; a statue stands on a wall
//  and is faced from a tile inside the map.
void check_gates(const MapData& map, const std::string& where){
    std::set<TilePos> in_gate;
    for (const auto& [id, gate] : map.gates){
        for (size_t c = 0; c < gate.tiles.size(); c++){
            const std::string tile_where = std::format("{}gates.{}.tiles[{}]", where, id, c);
            check_inside(map.tiles, gate.tiles[c], tile_where);
            if (map.tiles.at(gate.tiles[c]) != TileKind::GATE){
                throw ParseException(std::format("{} {} is not a G tile.", tile_where, to_string(gate.tiles[c])));
            }
            if (!in_gate.insert(gate.tiles[c]).second){
                throw ParseException(std::format("{} {} is in another gate too.", tile_where, to_string(gate.tiles[c])));
            }
        }
        if (gate.kind == GateKind::SWITCH && !map.switches.contains(gate.switch_id)){
            throw ParseException(std::format("{}gates.{}.switch \"{}\" is not in switches.", where, id, gate.switch_id));
        }
    }
    for (int y = 0; y < map.tiles.height; y++){
        for (int x = 0; x < map.tiles.width; x++){
            if (map.tiles.at({x, y}) == TileKind::GATE && !in_gate.contains(TilePos{x, y})){
                throw ParseException(std::format("{}tiles has a G at {} that no gate lists.", where, to_string(TilePos{x, y})));
            }
        }
    }
    for (const auto& [id, result] : map.switches){
        for (size_t c = 0; c < result.statues.size(); c++){
            const std::string statue_where = std::format("{}switches.{}.statues[{}]", where, id, c);
            check_inside(map.tiles, result.statues[c].tile, statue_where);
            check_inside(map.tiles, result.statues[c].face_from, statue_where);
        }
        for (size_t c = 0; c < result.buttons.size(); c++){
            check_inside(map.tiles, result.buttons[c], std::format("{}switches.{}.buttons[{}]", where, id, c));
        }
    }
}

} // namespace


MapData parse_map(const JsonValue& json, const std::string& source_name){
    const JsonValue normalized = normalize_runtime_map(json, source_name);
    const JsonObject& root = as_object(normalized, source_name);
    const std::string where = std::format("{}: ", source_name);
    MapData map;
    map.name = read_string(root, "name", where);
    map.source = read_source(root, where);
    map.world = read_world(root, where);
    map.allow_running = optional_bool(root, "allow_running", true, where);
    map.tiles = read_tiles(root, where);
    map.elevation = read_elevation(root, map.tiles, where);
    map.points = read_points(root, where);
    map.regions = read_regions(root, where);
    map.warps = read_list<MapWarp>(root, "warps", where, read_warp);
    map.objects = read_list<MapObject>(root, "objects", where, read_object);
    map.triggers = read_list<MapTrigger>(root, "triggers", where, read_trigger);
    map.flag_sets = read_flag_sets(root, where);
    map.connections = read_list<MapConnection>(root, "connections", where, read_connection);
    map.patches = read_list<MapPatch>(root, "patches", where, read_patch);
    map.gates = read_gates(root, where);
    map.switches = read_switches(root, where);
    check_places(map, where);
    for (const MapPatch& patch : map.patches){
        cell_at(TilePos{patch.x, patch.y}, map.tiles) = patch.glyph;
    }
    check_gates(map, where);
    return map;
}

const MapWarp* warp_at(const MapData& map, const TilePos& pos){
    for (const MapWarp& warp : map.warps){
        if (warp.x == pos.x && warp.y == pos.y){
            return &warp;
        }
    }
    return nullptr;
}

size_t tiles_to_door(const Grid<TileKind>& tiles, const TilePos& from, Direction direction, size_t max_tiles){
    TilePos at = from;
    size_t count = 0;
    while (count < max_tiles){
        const TilePos next = at.next(direction);
        if (!tiles.contains(next)){
            break;
        }
        count++;
        const TileKind kind = tiles.at(next);
        if (kind == TileKind::DOOR || kind == TileKind::WALL || kind == TileKind::OUTSIDE){
            break;
        }
        at = next;
    }
    return count;
}

size_t tiles_to_door(const MapData& map, const TilePos& from, Direction direction, size_t max_tiles){
    return tiles_to_door(map.tiles, from, direction, max_tiles);
}



} // namespace Pokemon
} // namespace PokemonAutomation
