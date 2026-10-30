/*  Overworld Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "PokemonFRLG_OverworldMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const char MAP_FOLDER[] = "PokemonFRLG/Maps/";

const char* const MAP_NAMES[] = {
    "CeruleanCity_PokemonCenter_1F",
    "CeruleanCity",
    "Route24",
    "CeruleanCave_1F",
    "CeruleanCave_2F",
    "CeruleanCave_B1F",
};


//  The readers below take `where`, the source and the path to the object
//  being read (e.g. "PokemonFRLG/Maps/CeruleanCity.json: warps[2]."), and
//  name the field in what they throw.

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

const JsonArray& read_array(const JsonObject& object, const std::string& key, const std::string& where){
    const JsonArray* array = read_field(object, key, where).to_array();
    if (array == nullptr){
        throw ParseException(where + key + " is not an array.");
    }
    return *array;
}

std::vector<std::string> read_rows(const JsonObject& object, const std::string& key, const std::string& where){
    std::vector<std::string> rows;
    const JsonArray& array = read_array(object, key, where);
    for (size_t c = 0; c < array.size(); c++){
        const std::string* row = array[c].to_string();
        if (row == nullptr){
            throw ParseException(where + key + "[" + std::to_string(c) + "] is not a string.");
        }
        rows.push_back(*row);
    }
    return rows;
}

MapWarp read_warp(const JsonObject& object, const std::string& where){
    MapWarp warp;
    warp.x = read_int(object, "x", where);
    warp.y = read_int(object, "y", where);
    warp.dest_map = read_string(object, "dest_map", where);
    warp.dest_warp = read_int(object, "dest_warp", where);
    return warp;
}

MapObject read_map_object(const JsonObject& object, const std::string& where){
    MapObject map_object;
    map_object.x = read_int(object, "x", where);
    map_object.y = read_int(object, "y", where);
    map_object.kind = read_string(object, "kind", where);
    map_object.flag = read_string(object, "flag", where);
    map_object.wander_x = read_int(object, "wander_x", where);
    map_object.wander_y = read_int(object, "wander_y", where);
    return map_object;
}

MapConnection read_connection(const JsonObject& object, const std::string& where){
    MapConnection connection;
    const std::string direction = read_string(object, "direction", where);
    bool known = false;
    for (Direction candidate : Pokemon::ALL_DIRECTIONS){
        if (direction == Pokemon::direction_name(candidate)){
            connection.direction = candidate;
            known = true;
        }
    }
    if (!known){
        throw ParseException(where + "direction \"" + direction + "\" is not north, east, south or west.");
    }
    connection.offset = read_int(object, "offset", where);
    connection.map = read_string(object, "map", where);
    return connection;
}

template <typename Item>
std::vector<Item> read_list(
    const JsonObject& object, const std::string& key, const std::string& where,
    Item (*read_item)(const JsonObject& object, const std::string& where)
){
    std::vector<Item> items;
    const JsonArray& array = read_array(object, key, where);
    for (size_t c = 0; c < array.size(); c++){
        const std::string item_where = where + key + "[" + std::to_string(c) + "]";
        items.push_back(read_item(as_object(array[c], item_where), item_where + "."));
    }
    return items;
}

MapData read_map(const JsonValue& json, const std::string& source){
    const JsonObject& root = as_object(json, source);
    const std::string where = source + ": ";
    MapData map;
    map.name = read_string(root, "name", where);
    map.width = read_int(root, "width", where);
    map.height = read_int(root, "height", where);
    map.tiles = read_rows(root, "tiles", where);
    map.elevation = read_rows(root, "elevation", where);
    map.warps = read_list<MapWarp>(root, "warps", where, read_warp);
    map.objects = read_list<MapObject>(root, "objects", where, read_map_object);
    map.connections = read_list<MapConnection>(root, "connections", where, read_connection);
    check_map(map);
    return map;
}

const std::vector<MapData>& all_maps(){
    static const std::vector<MapData> MAPS = []{
        std::vector<MapData> maps;
        for (const char* name : MAP_NAMES){
            maps.push_back(load_map(name));
        }
        return maps;
    }();
    return MAPS;
}

}


MapData parse_map(const std::string& json, const std::string& source){
    return read_map(parse_json(json), source);
}

MapData load_map(const std::string& name){
    const std::string source = std::string(MAP_FOLDER) + name + ".json";
    MapData map = read_map(load_json_file(RESOURCE_PATH() + source), source);
    if (map.name != name){
        throw ParseException(source + ": name is \"" + map.name + "\", not \"" + name + "\".");
    }
    return map;
}

std::vector<MapData> overworld_maps(const std::set<std::string>& set_flags){
    std::vector<MapData> maps = all_maps();
    for (MapData& map : maps){
        hide_objects(map, set_flags);
    }
    return maps;
}

const std::set<std::string>& post_game_flags(){
    static const std::set<std::string> FLAGS{
        "FLAG_HIDE_CERULEAN_CAVE_GUARD",
        "FLAG_HIDE_CERULEAN_RIVAL",
        "FLAG_HIDE_CERULEAN_ROCKET",
    };
    return FLAGS;
}



}
}
}
