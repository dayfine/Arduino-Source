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
    //  From a new game to Brock.
    "PalletTown",
    "PalletTown_PlayersHouse_1F",
    "PalletTown_PlayersHouse_2F",
    "PalletTown_RivalsHouse",
    "PalletTown_ProfessorOaksLab",
    "Route1",
    "ViridianCity",
    "ViridianCity_Mart",
    "ViridianCity_PokemonCenter_1F",
    "Route22",
    "Route2",
    "Route2_ViridianForest_SouthEntrance",
    "ViridianForest",
    "Route2_ViridianForest_NorthEntrance",
    "PewterCity",
    "PewterCity_PokemonCenter_1F",
    "PewterCity_Mart",
    "PewterCity_Museum_1F",
    "PewterCity_Gym",
    //  Cerulean Cave, for Mewtwo.
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

//  The grid `key`: `height` strings of `width` characters, each read by
//  `read_cell`, which gives nothing for a character it doesn't know.
template <typename Cell>
Grid<Cell> read_grid(
    const JsonObject& object, const std::string& key, const std::string& where,
    int width, int height, std::optional<Cell> (*read_cell)(char c)
){
    const JsonArray& rows = read_array(object, key, where);
    if ((int)rows.size() != height){
        throw ParseException(where + key + " has " + std::to_string(rows.size()) + " rows, not " + std::to_string(height) + ".");
    }
    Grid<Cell> grid{width, height, {}};
    for (int y = 0; y < height; y++){
        const std::string row_where = where + key + "[" + std::to_string(y) + "]";
        const std::string* row = rows[y].to_string();
        if (row == nullptr || (int)row->size() != width){
            throw ParseException(row_where + " is not a string of " + std::to_string(width) + " tiles.");
        }
        for (int x = 0; x < width; x++){
            const std::optional<Cell> cell = read_cell((*row)[x]);
            if (!cell){
                throw ParseException(
                    row_where + " has unknown tile '" + std::string(1, (*row)[x]) + "' at " + to_string(TilePos{x, y}) + "."
                );
            }
            grid.cells.push_back(*cell);
        }
    }
    return grid;
}

//  An elevation digit's level.
std::optional<uint8_t> elevation_from_char(char digit){
    if ('0' <= digit && digit <= '9'){
        return (uint8_t)(digit - '0');
    }
    if ('a' <= digit && digit <= 'f'){
        return (uint8_t)(digit - 'a' + 10);
    }
    return std::nullopt;
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

std::pair<std::string, TilePos> read_point(const JsonObject& object, const std::string& where){
    const std::string name = read_string(object, "name", where);
    return {name, TilePos{read_int(object, "x", where), read_int(object, "y", where)}};
}

std::pair<std::string, std::set<std::string>> read_flag_set(const JsonObject& object, const std::string& where){
    const std::string name = read_string(object, "name", where);
    std::set<std::string> flags;
    //  Named, since GCC warns of a reference returned for a temporary key.
    const std::string key = "flags";
    const JsonArray& array = read_array(object, key, where);
    for (size_t c = 0; c < array.size(); c++){
        const std::string* flag = array[c].to_string();
        if (flag == nullptr){
            throw ParseException(where + key + "[" + std::to_string(c) + "] is not a string.");
        }
        flags.insert(*flag);
    }
    return {name, std::move(flags)};
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
    const int width = read_int(root, "width", where);
    const int height = read_int(root, "height", where);
    if (width <= 0 || height <= 0){
        throw ParseException(where + "width and height must be positive.");
    }
    map.tiles = read_grid<Tile>(root, "tiles", where, width, height, tile_from_char);
    map.elevation = read_grid<uint8_t>(root, "elevation", where, width, height, elevation_from_char);
    map.warps = read_list<MapWarp>(root, "warps", where, read_warp);
    map.objects = read_list<MapObject>(root, "objects", where, read_map_object);
    map.connections = read_list<MapConnection>(root, "connections", where, read_connection);
    for (auto& point : read_list<std::pair<std::string, TilePos>>(root, "points", where, read_point)){
        map.points.insert(std::move(point));
    }
    for (auto& flag_set : read_list<std::pair<std::string, std::set<std::string>>>(root, "flag_sets", where, read_flag_set)){
        map.flag_sets.insert(std::move(flag_set));
    }
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

TilePos map_point(const std::vector<MapData>& maps, const std::string& map, const std::string& point){
    for (const MapData& data : maps){
        if (data.name != map){
            continue;
        }
        const auto iter = data.points.find(point);
        if (iter != data.points.end()){
            return iter->second;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No point \"" + point + "\" on map " + map + ".");
}

std::set<std::string> map_flag_set(const std::vector<MapData>& maps, const std::string& name){
    std::set<std::string> flags;
    for (const MapData& map : maps){
        const auto iter = map.flag_sets.find(name);
        if (iter != map.flag_sets.end()){
            flags.insert(iter->second.begin(), iter->second.end());
        }
    }
    return flags;
}



}
}
}
