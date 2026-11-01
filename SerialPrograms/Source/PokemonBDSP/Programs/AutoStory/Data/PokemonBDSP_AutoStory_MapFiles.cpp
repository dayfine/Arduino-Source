/*  BDSP AutoStory - Map files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "PokemonBDSP_AutoStory_MapGrids.h"
#include "PokemonBDSP_AutoStory_Maps_TestData.h"
#include "PokemonBDSP_AutoStory_MapFiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::MapData;
using Pokemon::TileGrid;


std::string story_map_folder(){
    return RESOURCE_PATH() + "PokemonBDSP/Maps/";
}

size_t load_story_map_files(const std::string& folder){
    std::map<std::string, TileGrid> grids;
    for (const auto& [name, json] : test_map_json()){
        const std::string path = folder + name + ".json";
        const MapData map = Pokemon::parse_map(load_json_file(path), path);
        if (map.name != name){
            throw ParseException(std::format("{}: name is \"{}\", not \"{}\".", path, map.name, name));
        }
        TileGrid grid = tile_grid_from_map(map);
        if (const std::optional<std::string> difference = grid_difference(grid, built_in_grid(name))){
            throw ParseException(std::format("{}: differs from this build's copy at {}.", path, *difference));
        }
        grids.emplace(name, std::move(grid));
    }
    const size_t count = grids.size();
    install_story_grids(std::move(grids));
    return count;
}



}
}
}
