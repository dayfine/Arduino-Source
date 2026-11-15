/*  BDSP AutoStory - Story map names
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The names the story worlds give their maps.
 *
 */
// code-health: allow header_symbols -- one list of every story map's name; splitting it hides which names are taken

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryMapNames_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryMapNames_H

#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Names of the maps in the story worlds (StoryMaps.h). Each is placed from the
//  JSON map named without spaces ("Route 201" from Route201.json), except
//  MAP_ROUTE_204, which is Route204North.json.
inline const std::string MAP_ROUTE_201 = "Route 201";
//  Sandgem Town overworld map name.
inline const std::string MAP_SANDGEM_TOWN = "Sandgem Town";
//  Route 202 overworld map name.
inline const std::string MAP_ROUTE_202 = "Route 202";
//  Jubilife City overworld map name.
inline const std::string MAP_JUBILIFE_CITY = "Jubilife City";
//  Oreburgh Mine map name.
inline const std::string MAP_OREBURGH_MINE = "Oreburgh Mine";
//  Ravaged Path map name.
inline const std::string MAP_RAVAGED_PATH = "Ravaged Path";
//  Route 204 North map name.
inline const std::string MAP_ROUTE_204 = "Route 204";
//  Route 204 South map name.
inline const std::string MAP_ROUTE_204_SOUTH = "Route 204 South";
//  Route 203 overworld map name.
inline const std::string MAP_ROUTE_203 = "Route 203";
//  Oreburgh City overworld map name.
inline const std::string MAP_OREBURGH_CITY = "Oreburgh City";
//  Floaroma Town overworld map name.
inline const std::string MAP_FLOAROMA_TOWN = "Floaroma Town";


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
