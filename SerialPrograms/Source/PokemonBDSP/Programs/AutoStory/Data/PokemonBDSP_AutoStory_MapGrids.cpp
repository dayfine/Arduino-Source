/*  BDSP AutoStory - Map grids
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonBDSP_AutoStory_MapGrids.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::MapData;
using Pokemon::TileGrid;
using Pokemon::TileKind;
using Pokemon::TilePos;
using Pokemon::TileType;


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

std::map<std::string, TileGrid>& installed_grids(){
    static std::map<std::string, TileGrid> grids;
    return grids;
}

const std::map<std::string, TileGrid>& built_in_grids(){
    static const std::map<std::string, TileGrid> grids = []{
        std::map<std::string, TileGrid> parsed;
        for (const auto& [name, json] : bdsp_map_json()){
            const MapData map = Pokemon::parse_map(parse_json(json), name + ".json (built in)");
            if (map.name != name){
                throw ParseException(std::format("{}.json (built in): name is \"{}\".", name, map.name));
            }
            parsed.emplace(name, tile_grid_from_map(map));
        }
        return parsed;
    }();
    return grids;
}

}

const TileGrid& built_in_grid(const std::string& name){
    const std::map<std::string, TileGrid>& grids = built_in_grids();
    if (auto iter = grids.find(name); iter != grids.end()){
        return iter->second;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No BDSP story map named \"" + name + "\".");
}

const TileGrid& story_grid(const std::string& name){
    const std::map<std::string, TileGrid>& grids = installed_grids();
    if (auto iter = grids.find(name); iter != grids.end()){
        return iter->second;
    }
    return built_in_grid(name);
}

void install_story_grids(std::map<std::string, TileGrid> grids){
    if (installed_grids().empty()){
        installed_grids() = std::move(grids);
    }
}



}
}
}
