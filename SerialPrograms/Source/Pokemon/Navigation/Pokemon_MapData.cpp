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
};

constexpr std::pair<const char*, WarpEntry> WARP_ENTRIES[] = {
    {"walk_into", WarpEntry::WALK_INTO},
    {"step_on", WarpEntry::STEP_ON},
    {"cave_entrance", WarpEntry::CAVE_ENTRANCE},
    {"walk_north", WarpEntry::WALK_NORTH},
    {"walk_south", WarpEntry::WALK_SOUTH},
    {"walk_west", WarpEntry::WALK_WEST},
    {"walk_east", WarpEntry::WALK_EAST},
};

}

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
        throw ParseException(where + " is not a JSON object.");
    }
    return *object;
}

const JsonValue& read_field(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue* value = object.get_value(key);
    if (value == nullptr){
        throw ParseException(where + key + " is missing.");
    }
    return *value;
}

int read_int(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue& value = read_field(object, key, where);
    if (!value.is_integer()){
        throw ParseException(where + key + " is not an integer.");
    }
    return (int)value.to_integer_default();
}

std::string read_string(const JsonObject& object, const std::string& key, const std::string& where){
    const std::string* text = read_field(object, key, where).to_string();
    if (text == nullptr){
        throw ParseException(where + key + " is not a string.");
    }
    return *text;
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
    return parse_direction(read_string(object, key, where), where + key);
}

//  The list `key`, or nothing when the map leaves it out.
const JsonArray* optional_array(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue* value = object.get_value(key);
    if (value == nullptr){
        return nullptr;
    }
    const JsonArray* array = value->to_array();
    if (array == nullptr){
        throw ParseException(where + key + " is not an array.");
    }
    return array;
}

const JsonObject* optional_object(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonValue* value = object.get_value(key);
    if (value == nullptr){
        return nullptr;
    }
    return &as_object(*value, where + key);
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
        items.push_back(read_item(as_object((*array)[c], item_where), item_where + "."));
    }
    return items;
}


MapSource read_source(const JsonObject& root, const std::string& where){
    //  Named, since GCC warns of a reference returned for a temporary path.
    const std::string key = "source";
    const std::string path = where + key;
    const JsonObject& object = as_object(read_field(root, key, where), path);
    const std::string source_where = path + ".";
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
    const std::string world_where = where + "world.";
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
        throw ParseException(where + key + " is not an array.");
    }
    if (rows->empty()){
        throw ParseException(where + key + " has no rows.");
    }
    const std::string* first = (*rows)[0].to_string();
    const size_t width = first == nullptr ? 0 : first->size();
    if (width == 0){
        throw ParseException(where + key + "[0] is not a string of glyphs.");
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
        throw ParseException(where + " is not [x, y].");
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
            throw ParseException(set_where + " is not a list of flags.");
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
            throw ParseException(region_where + " is not a non-empty list of [x, y].");
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
    const bool has_map = object.get_value("to_map") != nullptr;
    const bool has_point = object.get_value("to_point") != nullptr;
    if (has_map != has_point){
        throw ParseException(where + (has_map ? "to_point" : "to_map") + " is missing.");
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
        throw ParseException(where.substr(0, where.size() - 1) + " is not a box (x0 <= x1, y0 <= y1).");
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
            throw ParseException(where + "looks is missing, and the object has no facing.");
        }
        trainer.looks.push_back(*facing);
        return trainer;
    }
    for (size_t c = 0; c < looks->size(); c++){
        const std::string item_where = std::format("{}looks[{}]", where, c);
        const std::string* name = (*looks)[c].to_string();
        if (name == nullptr){
            throw ParseException(item_where + " is not a string.");
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
        map_object.wander = read_box(*wander, where + "wander.");
    }
    if (const JsonObject* trainer = optional_object(object, "trainer", where)){
        map_object.trainer = read_trainer(*trainer, map_object.facing, where + "trainer.");
    }
    map_object.hide_flag = optional_string(object, "hide_flag", where);
    return map_object;
}

MapTrigger read_trigger(const JsonObject& object, const std::string& where){
    MapTrigger trigger;
    trigger.name = read_string(object, "name", where);
    //  Named, since GCC warns of a reference returned for a temporary path.
    const std::string key = "box";
    trigger.box = read_box(as_object(read_field(object, key, where), where + key), where + key + ".");
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
        check_inside(map.tiles, pos, where + "points." + name);
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

}


MapData parse_map(const JsonValue& json, const std::string& source_name){
    const JsonObject& root = as_object(json, source_name);
    const std::string where = source_name + ": ";
    MapData map;
    map.name = read_string(root, "name", where);
    map.source = read_source(root, where);
    map.world = read_world(root, where);
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
    check_places(map, where);
    for (const MapPatch& patch : map.patches){
        cell_at(map.tiles, TilePos{patch.x, patch.y}) = patch.glyph;
    }
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



}
}
