/*  Overworld Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The FRLG maps the overworld routes know about so far: Cerulean City, its
 *  Pokemon Center, Route 24 to its north, and the three floors of Cerulean
 *  Cave, where Mewtwo waits on B1F.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldMaps_H
#define PokemonAutomation_PokemonFRLG_OverworldMaps_H

#include <set>
#include <string>
#include <vector>
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Every map, by its decompilation name ("CeruleanCity", "CeruleanCave_B1F", ...),
//  with the objects the given event flags hide removed.
std::vector<OverworldMap> overworld_maps(const std::set<std::string>& set_flags = {});

//  Flags set once the player has entered the Hall of Fame, which clear the
//  way to Cerulean Cave: its guard, the rival and the Rocket in Cerulean City.
const std::set<std::string>& post_game_flags();

//  Where the player stands after entering Cerulean Cave from Cerulean City (1F).
constexpr TilePos CERULEAN_CAVE_ENTRANCE{33, 21};

//  Mewtwo's tile on B1F. Its battle starts by talking to it from the south.
constexpr TilePos CERULEAN_CAVE_MEWTWO{7, 12};

//  The tile in front of the PC in Cerulean City's Pokemon Center.
constexpr TilePos CERULEAN_POKEMON_CENTER_PC{11, 2};



}
}
}
#endif
