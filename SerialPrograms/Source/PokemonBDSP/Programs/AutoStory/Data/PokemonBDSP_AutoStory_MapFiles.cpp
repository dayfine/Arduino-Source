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
#include "PokemonBDSP_AutoStory_FloaromaTown.h"
#include "PokemonBDSP_AutoStory_JubilifeCity.h"
#include "PokemonBDSP_AutoStory_LakeVerity.h"
#include "PokemonBDSP_AutoStory_OreburghCity.h"
#include "PokemonBDSP_AutoStory_OreburghGate.h"
#include "PokemonBDSP_AutoStory_OreburghGym.h"
#include "PokemonBDSP_AutoStory_OreburghMine.h"
#include "PokemonBDSP_AutoStory_RavagedPath.h"
#include "PokemonBDSP_AutoStory_Route201.h"
#include "PokemonBDSP_AutoStory_Route202.h"
#include "PokemonBDSP_AutoStory_Route203.h"
#include "PokemonBDSP_AutoStory_Route204.h"
#include "PokemonBDSP_AutoStory_Route204South.h"
#include "PokemonBDSP_AutoStory_SandgemTown.h"
#include "PokemonBDSP_AutoStory_TwinleafTown.h"
#include "PokemonBDSP_AutoStory_VerityLakefront.h"
#include "PokemonBDSP_AutoStory_MapFiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::MapData;
using Pokemon::TileGrid;
using Pokemon::TileKind;
using Pokemon::TilePos;
using Pokemon::TileType;


const std::vector<StoryMapFile>& story_map_files(){
    static const std::vector<StoryMapFile> files{
        {"TwinleafTown", TwinleafTown::grid},
        {"VerityLakefront", VerityLakefront::grid},
        {"LakeVerity", LakeVerity::grid},
        {"Route201", Route201::grid},
        {"SandgemTown", SandgemTown::grid},
        {"Route202", Route202::grid},
        {"JubilifeCity", JubilifeCity::grid},
        {"Route203", Route203::grid},
        {"OreburghGate", OreburghGate::grid},
        {"OreburghCity", OreburghCity::grid},
        {"OreburghMine", OreburghMine::grid},
        {"OreburghGym", OreburghGym::grid},
        {"Route204South", Route204South::grid},
        {"RavagedPath", RavagedPath::grid},
        {"Route204North", Route204::north_grid},
        {"FloaromaTown", FloaromaTown::grid},
    };
    return files;
}

std::string story_map_folder(){
    return RESOURCE_PATH() + "PokemonBDSP/Maps/";
}


namespace{

std::optional<TileType> tile_type_of(TileKind kind){
    switch (kind){
    case TileKind::WALL:        return TileType::BLOCKED;
    case TileKind::OPEN:        return TileType::WALKABLE;
    case TileKind::TALL_GRASS:  return TileType::TALL_GRASS;
    case TileKind::LEDGE_SOUTH: return TileType::LEDGE;
    case TileKind::DOOR:        return TileType::DOOR;
    case TileKind::SIGN:        return TileType::SIGN;
    default:                    return std::nullopt;
    }
}

}

TileGrid tile_grid_from_map(const MapData& map){
    std::vector<std::string> rows;
    for (int y = 0; y < map.tiles.height; y++){
        std::string row;
        for (int x = 0; x < map.tiles.width; x++){
            const TileKind kind = map.tiles.at(TilePos{x, y});
            const std::optional<TileType> type = tile_type_of(kind);
            if (!type){
                throw ParseException(std::format(
                    "{}: glyph '{}' at {} has no BDSP tile.", map.name, Pokemon::glyph_of(kind), Pokemon::to_string(TilePos{x, y})
                ));
            }
            row += Pokemon::tile_char(*type);
        }
        rows.push_back(std::move(row));
    }
    for (const Pokemon::MapObject& object : map.objects){
        if (object.kind != Pokemon::ObjectKind::NPC || object.wander){
            throw ParseException(std::format("{}: object {} is not a person standing still.", map.name, object.name));
        }
        rows[object.y][object.x] = Pokemon::tile_char(TileType::NPC);
    }
    return TileGrid(rows);
}

std::optional<std::string> grid_difference(const TileGrid& here, const TileGrid& there){
    if (here.width() != there.width() || here.height() != there.height()){
        return std::format("{} x {} here, {} x {} there", here.width(), here.height(), there.width(), there.height());
    }
    for (int y = 0; y < here.height(); y++){
        for (int x = 0; x < here.width(); x++){
            const TilePos pos{x, y};
            if (here.at(pos) != there.at(pos)){
                return std::format(
                    "{}: '{}' here, '{}' there",
                    Pokemon::to_string(pos), Pokemon::tile_char(here.at(pos)), Pokemon::tile_char(there.at(pos))
                );
            }
        }
    }
    return std::nullopt;
}


namespace{

std::map<std::string, TileGrid>& loaded_grids(){
    static std::map<std::string, TileGrid> grids;
    return grids;
}

}

void load_story_map_files(const std::string& folder){
    std::map<std::string, TileGrid> grids;
    for (const StoryMapFile& file : story_map_files()){
        const std::string path = folder + file.name + ".json";
        const MapData map = Pokemon::parse_map(load_json_file(path), path);
        if (map.name != file.name){
            throw ParseException(std::format("{}: name is \"{}\", not \"{}\".", path, map.name, file.name));
        }
        TileGrid grid = tile_grid_from_map(map);
        if (const std::optional<std::string> difference = grid_difference(grid, file.table())){
            throw ParseException(std::format("{}: differs from the C++ table at {}.", path, *difference));
        }
        grids.emplace(file.name, std::move(grid));
    }
    //  Once only: StoryMaps keeps copies of what it was handed first, and a
    //  second load can only find the same files equal to the same tables.
    if (loaded_grids().empty()){
        loaded_grids() = std::move(grids);
    }
}

const TileGrid& story_grid(const std::string& name){
    const std::map<std::string, TileGrid>& grids = loaded_grids();
    if (auto iter = grids.find(name); iter != grids.end()){
        return iter->second;
    }
    for (const StoryMapFile& file : story_map_files()){
        if (file.name == name){
            return file.table();
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No BDSP story map named \"" + name + "\".");
}



}
}
}
