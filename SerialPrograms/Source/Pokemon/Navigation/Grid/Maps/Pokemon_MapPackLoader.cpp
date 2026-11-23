/*  Map Pack Loader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_MapPackLoader.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

std::string map_source_name(std::string_view prefix, std::string_view name){
    return std::format("{}{}.json", prefix, name);
}

MapData parse_checked_map(
    std::string_view name,
    const std::string& json_text,
    std::string_view source_prefix
){
    const std::string source = map_source_name(source_prefix, name);
    MapData map = parse_map(parse_json(json_text), source);
    if (map.name != name){
        throw ParseException(std::format("{}: name is \"{}\", not \"{}\".", source, map.name, name));
    }
    return map;
}

bool box_overlaps_crop(const TileBox& box, int width, int height){
    return box.x1 >= 0 && box.y1 >= 0 && box.x0 < width && box.y0 < height;
}

} // namespace


MapData load_embedded_map(
    std::span<const std::pair<std::string, std::string>> embedded_json,
    std::string_view name,
    std::string_view source_prefix
){
    const auto iter = std::find_if(
        embedded_json.begin(), embedded_json.end(),
        [name](const auto& entry){ return entry.first == name; }
    );
    if (iter == embedded_json.end()){
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            std::format("{} is not built into the program.", map_source_name(source_prefix, name))
        );
    }
    return parse_checked_map(name, iter->second, source_prefix);
}

std::map<std::string, MapData, std::less<>> load_embedded_map_pack(
    std::span<const std::pair<std::string, std::string>> embedded_json,
    std::string_view source_prefix
){
    std::map<std::string, MapData, std::less<>> parsed;
    for (const auto& [name, json_text] : embedded_json){
        parsed.emplace(name, parse_checked_map(name, json_text, source_prefix));
    }
    return parsed;
}

TilePos require_map_point(const MapData& map, std::string_view point){
    if (auto iter = map.points.find(std::string(point)); iter != map.points.end()){
        return iter->second;
    }
    throw InternalProgramError(
        nullptr, PA_CURRENT_FUNCTION,
        std::format("{} has no point \"{}\".", map.name, point)
    );
}

TilePos require_map_point(
    std::span<const MapData> maps,
    std::string_view map_name,
    std::string_view point
){
    for (const MapData& map : maps){
        if (map.name == map_name){
            return require_map_point(map, point);
        }
    }
    throw InternalProgramError(
        nullptr, PA_CURRENT_FUNCTION,
        std::format("No point \"{}\" on map {}.", point, map_name)
    );
}

std::vector<TilePos> require_numbered_map_region(
    const MapData& map,
    std::string_view region
){
    std::vector<TilePos> tiles;
    while (true){
        const auto iter = map.points.find(std::format("{}.{}", region, tiles.size() + 1));
        if (iter == map.points.end()){
            break;
        }
        tiles.push_back(iter->second);
    }
    if (tiles.empty()){
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            std::format("{} has no region \"{}\".", map.name, region)
        );
    }
    return tiles;
}

std::set<std::string> collect_map_flag_set(
    std::span<const MapData> maps,
    std::string_view flag_set_name
){
    std::set<std::string> flags;
    const std::string key(flag_set_name);
    for (const MapData& map : maps){
        const auto iter = map.flag_sets.find(key);
        if (iter != map.flag_sets.end()){
            flags.insert(iter->second.begin(), iter->second.end());
        }
    }
    return flags;
}

MapData crop_map_data(
    const MapData& map,
    const TilePos& corner,
    int width,
    int height
){
    MapData cropped;
    cropped.name = map.name;
    cropped.source = map.source;
    cropped.allow_running = map.allow_running;
    cropped.tiles = Grid<TileKind>{width, height, {}};
    if (map.elevation){
        cropped.elevation = Grid<uint8_t>{width, height, {}};
    }
    for (int y = 0; y < height; y++){
        for (int x = 0; x < width; x++){
            const TilePos src{corner.x + x, corner.y + y};
            cropped.tiles.cells.push_back(map.tiles.at(src));
            if (cropped.elevation){
                cropped.elevation->cells.push_back(map.elevation->at(src));
            }
        }
    }
    for (MapObject object : map.objects){
        object.x -= corner.x;
        object.y -= corner.y;
        if (object.wander){
            object.wander->x0 -= corner.x;
            object.wander->x1 -= corner.x;
            object.wander->y0 -= corner.y;
            object.wander->y1 -= corner.y;
        }
        if (cropped.tiles.contains({object.x, object.y})
            || (object.wander && box_overlaps_crop(*object.wander, width, height))
        ){
            cropped.objects.push_back(std::move(object));
        }
    }
    for (MapWarp warp : map.warps){
        warp.x -= corner.x;
        warp.y -= corner.y;
        if (cropped.tiles.contains({warp.x, warp.y})){
            cropped.warps.push_back(std::move(warp));
        }
    }
    return cropped;
}



} // namespace Pokemon
} // namespace PokemonAutomation
