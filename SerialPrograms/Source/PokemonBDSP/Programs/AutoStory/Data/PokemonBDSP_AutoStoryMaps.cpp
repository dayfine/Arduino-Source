/*  BDSP AutoStory - Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_MapPackLoader.h"
#include "PokemonBDSP_AutoStoryMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::MapData;
using Pokemon::TilePos;


namespace{

const std::map<std::string, MapData, std::less<>>& story_maps_by_name(){
    static const std::map<std::string, MapData, std::less<>> maps = Pokemon::load_embedded_map_pack(bdsp_map_json());
    return maps;
}

} // namespace


const MapData& story_map(std::string_view name){
    const auto& maps = story_maps_by_name();
    if (auto iter = maps.find(name); iter != maps.end()){
        return iter->second;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No BDSP story map named \"{}\".", name));
}

TilePos map_point(std::string_view map, std::string_view point){
    return Pokemon::require_map_point(story_map(map), point);
}

std::vector<TilePos> map_region(std::string_view map, std::string_view region){
    return Pokemon::require_numbered_map_region(story_map(map), region);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
