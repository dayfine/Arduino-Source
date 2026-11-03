/*  Overworld Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonFRLG_OverworldMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

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
    return Pokemon::parse_map(parse_json(json), source);
}

MapData load_map(const std::string& name){
    const std::string source = "Maps/" + name + ".json";
    const std::vector<std::pair<std::string, std::string>>& files = overworld_map_json();
    const auto file = std::find_if(files.begin(), files.end(), [&](const auto& item){ return item.first == name; });
    if (file == files.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, source + " is not built into the program.");
    }
    MapData map = parse_map(file->second, source);
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
