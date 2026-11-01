/*  Overworld Test Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The FRLG maps built into the program, for tests: CI has none of the map
 *  files in Packages. The .json files in Maps/ next to this file are what
 *  devtools/maps/frlg_maps.py writes, the same as Resources/PokemonFRLG/Maps
 *  in Packages; `frlg_maps.py --out <Maps folder> --check` checks them.
 *  CMake builds their text in (pa_embed_text_files() in
 *  cmake/EmbedTextFiles.cmake).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldTestMaps_H
#define PokemonAutomation_PokemonFRLG_OverworldTestMaps_H

#include <set>
#include <string>
#include <utility>
#include <vector>
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  (name, JSON text) of each file in Maps/, sorted by name. Generated at
//  build time.
const std::vector<std::pair<std::string, std::string>>& overworld_map_json();

//  overworld_maps(), read from overworld_map_json() rather than from the
//  files. Throws ParseException as parse_map().
std::vector<MapData> test_overworld_maps(const std::set<std::string>& set_flags = {});



}
}
}
#endif
