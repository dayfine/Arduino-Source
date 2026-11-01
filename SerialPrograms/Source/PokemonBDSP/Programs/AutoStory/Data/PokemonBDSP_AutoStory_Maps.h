/*  BDSP AutoStory - Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The maps the story walks, as JSON files in the shared map schema
 *  (Pokemon_MapData.h): PokemonBDSP/Programs/AutoStory/Maps, written by
 *  devtools/maps/bdsp_maps.py from the Diamond decompilation and built into
 *  the program by CMake. Each is named as its file ("Route202").
 *
 *  A map's named tiles are its points. A region, a list of tiles, is the
 *  points "<region>.1", "<region>.2", ... in order: the schema has
 *  single-tile points only.
 *
 *  TODO: move the maps to PokemonAutomation/Packages (Resources/PokemonBDSP/Maps) when ready
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Maps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Maps_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  (name, JSON text) of each map in PokemonBDSP/Programs/AutoStory/Maps/,
//  built into the program by pa_embed_text_files() (cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& bdsp_map_json();

//  The map `name`, read from bdsp_map_json() on first use, with its patches
//  applied. Throws InternalProgramError for a name it does not have, and
//  ParseException if a map does not parse or is not named as its file.
//  The name is a string_view so that a temporary string argument is not
//  mistaken (by GCC's -Wdangling-reference) for what the result refers to.
const Pokemon::MapData& story_map(std::string_view name);

//  The point `point` of the map `map`. Throws InternalProgramError for an
//  unknown map or point.
Pokemon::TilePos map_point(std::string_view map, std::string_view point);

//  The tiles of the region `region` of the map `map`, in order. Throws
//  InternalProgramError for an unknown map, or a region with no tiles.
std::vector<Pokemon::TilePos> map_region(std::string_view map, std::string_view region);



}
}
}
#endif
