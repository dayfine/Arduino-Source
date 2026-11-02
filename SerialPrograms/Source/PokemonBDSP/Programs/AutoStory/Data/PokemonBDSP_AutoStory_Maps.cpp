/*  BDSP AutoStory - Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonBDSP_AutoStory_Maps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::MapData;
using Pokemon::TilePos;


namespace{

const std::map<std::string, MapData, std::less<>>& story_maps_by_name(){
    static const std::map<std::string, MapData, std::less<>> maps = []{
        std::map<std::string, MapData, std::less<>> parsed;
        for (const auto& [name, json] : bdsp_map_json()){
            MapData map = Pokemon::parse_map(parse_json(json), name + ".json");
            if (map.name != name){
                throw ParseException(std::format("{}.json: name is \"{}\".", name, map.name));
            }
            parsed.emplace(name, std::move(map));
        }
        return parsed;
    }();
    return maps;
}

}


const MapData& story_map(std::string_view name){
    const auto& maps = story_maps_by_name();
    if (auto iter = maps.find(name); iter != maps.end()){
        return iter->second;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No BDSP story map named \"{}\".", name));
}

TilePos map_point(std::string_view map, std::string_view point){
    const MapData& data = story_map(map);
    if (auto iter = data.points.find(std::string(point)); iter != data.points.end()){
        return iter->second;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("{} has no point \"{}\".", map, point));
}

std::vector<TilePos> map_region(std::string_view map, std::string_view region){
    const MapData& data = story_map(map);
    std::vector<TilePos> tiles;
    while (true){
        const auto iter = data.points.find(std::format("{}.{}", region, tiles.size() + 1));
        if (iter == data.points.end()){
            break;
        }
        tiles.push_back(iter->second);
    }
    if (tiles.empty()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("{} has no region \"{}\".", map, region));
    }
    return tiles;
}



}
}
}
