/*  Cerulean Cave Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The three floors of Cerulean Cave, where Mewtwo waits on B1F.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_CeruleanCaveMaps_H
#define PokemonAutomation_PokemonFRLG_CeruleanCaveMaps_H

#include <vector>
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  1F, 2F and B1F, by their decompilation names ("CeruleanCave_1F", ...).
const std::vector<OverworldMap>& cerulean_cave_maps();

//  Where the player stands after entering from Cerulean City (1F).
constexpr MapPoint CERULEAN_CAVE_ENTRANCE{33, 21};

//  Mewtwo's tile on B1F. Its battle starts by talking to it from the south.
constexpr MapPoint CERULEAN_CAVE_MEWTWO{7, 12};



}
}
}
#endif
