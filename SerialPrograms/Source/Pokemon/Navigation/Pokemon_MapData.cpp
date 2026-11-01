/*  Map Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
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

TilePos read_pos(const JsonObject& object, const std::string& where){
    return TilePos{read_int(object, "x", where), read_int(object, "y", where)};
}

Direction read_direction(const JsonObject& object, const std::string& key, const std::string& where){
    const std::string name = read_string(object, key, where);
    for (Direction candidate : ALL_DIRECTIONS){
        if (name == direction_name(candidate)){
            return candidate;
        }
    }
    throw ParseException(std::format("{}{} \"{}\" is not north, east, south or west.", where, key, name));
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
    const std::string key = "source";
    const JsonObject& object = as_object(read_field(root, key, where), where + key);
    const std::string source_where = where + key + ".";
    MapSource source;
    source.decomp_file = read_string(object, "decomp_file", source_where);
    source.revision = read_string(object, "revision", source_where);
    source.date = read_string(object, "date", source_where);
    return source;
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

std::map<std::string, TilePos> read_points(const JsonObject& root, const std::string& where){
    std::map<std::string, TilePos> points;
    const JsonObject* object = optional_object(root, "points", where);
    if (object == nullptr){
        return points;
    }
    for (const auto& [name, value] : *object){
        const JsonArray* pair = value.to_array();
        if (pair == nullptr || pair->size() != 2 || !(*pair)[0].is_integer() || !(*pair)[1].is_integer()){
            throw ParseException(std::format("{}points.{} is not [x, y].", where, name));
        }
        points[name] = TilePos{(int)(*pair)[0].to_integer_default(), (int)(*pair)[1].to_integer_default()};
    }
    return points;
}

MapWarp read_warp(const JsonObject& object, const std::string& where){
    MapWarp warp;
    const TilePos pos = read_pos(object, where);
    warp.x = pos.x;
    warp.y = pos.y;
    warp.to_map = read_string(object, "to_map", where);
    warp.to_point = read_string(object, "to_point", where);
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

WanderBox read_wander(const JsonObject& object, const std::string& where){
    WanderBox box;
    box.x0 = read_int(object, "x0", where);
    box.y0 = read_int(object, "y0", where);
    box.x1 = read_int(object, "x1", where);
    box.y1 = read_int(object, "y1", where);
    if (box.x0 > box.x1 || box.y0 > box.y1){
        throw ParseException(where.substr(0, where.size() - 1) + " is not a box (x0 <= x1, y0 <= y1).");
    }
    return box;
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
        map_object.wander = read_wander(*wander, where + "wander.");
    }
    return map_object;
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

//  Everything with a place on the map has it inside the tiles.
void check_places(const MapData& map, const std::string& where){
    for (const auto& [name, pos] : map.points){
        check_inside(map.tiles, pos, where + "points." + name);
    }
    for (size_t c = 0; c < map.warps.size(); c++){
        check_inside(map.tiles, TilePos{map.warps[c].x, map.warps[c].y}, std::format("{}warps[{}]", where, c));
    }
    for (size_t c = 0; c < map.objects.size(); c++){
        const MapObject& object = map.objects[c];
        check_inside(map.tiles, TilePos{object.x, object.y}, std::format("{}objects[{}]", where, c));
        if (object.wander){
            const std::string wander_where = std::format("{}objects[{}].wander", where, c);
            check_inside(map.tiles, TilePos{object.wander->x0, object.wander->y0}, wander_where);
            check_inside(map.tiles, TilePos{object.wander->x1, object.wander->y1}, wander_where);
        }
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
    map.tiles = read_tiles(root, where);
    map.points = read_points(root, where);
    map.warps = read_list<MapWarp>(root, "warps", where, read_warp);
    map.objects = read_list<MapObject>(root, "objects", where, read_object);
    map.connections = read_list<MapConnection>(root, "connections", where, read_connection);
    map.patches = read_list<MapPatch>(root, "patches", where, read_patch);
    check_places(map, where);
    for (const MapPatch& patch : map.patches){
        map.tiles.at(TilePos{patch.x, patch.y}) = patch.glyph;
    }
    return map;
}



}
}
