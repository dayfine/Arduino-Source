/*  BDSP AutoStory - Pokemon Center
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_PokemonCenter_H
#define PokemonAutomation_PokemonBDSP_AutoStory_PokemonCenter_H

#include <string>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Heals the party at a Pokemon Center. Starts on the row below the Center's
//  front, a few tiles from its door in `door_side`: tries north, and steps a
//  tile toward the door each time a wall stops it. Inside, walks up to the
//  nurse, accepts, and walks back out. Ends on the tile below the door.
void heal_at_pokemon_center(
    VideoStream& stream, ProControllerContext& context,
    Pokemon::GridDirection door_side, const std::string& label
);



}
}
}
#endif
