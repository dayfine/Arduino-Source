/*  BDSP AutoStory - Oreburgh Gym tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Gym's inside, decoded from the Pokemon Diamond decompilation
 *  (pret/pokediamond); see the .cpp's header for the cell, the offset and
 *  the objects. For checkpoint 01.04's walk up to Roark.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_OreburghGym_H
#define PokemonAutomation_PokemonBDSP_AutoStory_OreburghGym_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace OreburghGym{


const Pokemon::TileGrid& grid();

//  Where coming in through the door puts us: the tile above it, beside the
//  Gym Guide. Roark stands at the top of the same column.
extern const Pokemon::TilePos ENTRANCE;
extern const Pokemon::TilePos ROARK;

//  The two Youngsters, who face the column up the middle from either side:
//  walking north from the entrance meets both.
std::vector<Pokemon::TrainerSight> trainer_sights();


}
}
}
}
#endif
