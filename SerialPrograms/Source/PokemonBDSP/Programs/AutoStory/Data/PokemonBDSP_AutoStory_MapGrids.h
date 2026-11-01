/*  BDSP AutoStory - Map grids
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's maps are JSON files in the shared map schema
 *  (Pokemon_MapData.h), PokemonBDSP/Programs/AutoStory/Maps, built into the
 *  program by CMake, read with the shared loader and turned into the tile
 *  grids the planner walks. Each map's X::grid() returns story_grid("X").
 *
 *  TODO: move the maps to PokemonAutomation/Packages (Resources/PokemonBDSP/Maps) when ready
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_MapGrids_H
#define PokemonAutomation_PokemonBDSP_AutoStory_MapGrids_H

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  (name, JSON text) of each map in PokemonBDSP/Programs/AutoStory/Maps/,
//  built into the program by pa_embed_text_files() (cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& bdsp_map_json();

//  A map's tiles as a tile grid: walls, open ground, tall grass, south
//  ledges, doors and signs; a standing person ("npc" object) is an NPC tile.
//  Throws ParseException, naming the map, for what BDSP's grids have no
//  tile for (water, ledges other than south, items, wanderers).
Pokemon::TileGrid tile_grid_from_map(const Pokemon::MapData& map);

//  "(x, y): 'N' here, '.' there", or nothing when the grids are the same
//  size and equal cell for cell.
std::optional<std::string> grid_difference(const Pokemon::TileGrid& here, const Pokemon::TileGrid& there);

//  The grid of the map `name`, read from bdsp_map_json() once. Throws
//  InternalProgramError for a name it does not have.
const Pokemon::TileGrid& story_grid(const std::string& name);



}
}
}
#endif
