/*  Overworld Maps Test Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The FRLG maps compiled into the program, for tests: CI runs without the
 *  map files in Packages. PokemonFRLG_OverworldMapsTestData.cpp holds the
 *  same JSON as the files in Resources/PokemonFRLG/Maps, written by
 *  devtools/maps/frlg_maps.py --cpp, which also checks it (--check).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldMapsTestData_H
#define PokemonAutomation_PokemonFRLG_OverworldMapsTestData_H

#include <set>
#include <string>
#include <utility>
#include <vector>
#include "PokemonFRLG_OverworldMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Every map's name and JSON text, as in its file in Packages.
const std::vector<std::pair<std::string, std::string>>& overworld_map_json();

//  overworld_maps(), read from overworld_map_json() rather than from the
//  files. Throws ParseException as parse_map().
std::vector<MapData> test_overworld_maps(const std::set<std::string>& set_flags = {});



}
}
}
#endif
