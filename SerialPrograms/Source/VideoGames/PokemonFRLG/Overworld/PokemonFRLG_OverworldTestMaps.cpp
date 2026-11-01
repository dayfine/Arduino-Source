/*  Overworld Test Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldTestMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::vector<MapData> test_overworld_maps(const std::set<std::string>& set_flags){
    static const std::vector<MapData> ALL = []{
        std::vector<MapData> maps;
        for (const auto& [name, json] : overworld_map_json()){
            const std::string source = "Maps/" + name + ".json (built in)";
            maps.push_back(parse_map(json, source));
            if (maps.back().name != name){
                throw ParseException(source + ": name is \"" + maps.back().name + "\".");
            }
        }
        return maps;
    }();
    std::vector<MapData> maps = ALL;
    for (MapData& map : maps){
        hide_objects(map, set_flags);
    }
    return maps;
}



}
}
}
