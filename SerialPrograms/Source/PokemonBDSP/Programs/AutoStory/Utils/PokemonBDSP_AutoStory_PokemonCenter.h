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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Heals the party at a Pokemon Center. Starts in front of the Center with its
//  lit doorway on screen, within a few tiles of it: lines up with the doorway
//  by sight, walks in, talks to the nurse, accepts, and walks back out. Ends
//  on the tile below the door.
void heal_at_pokemon_center(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
);



}
}
}
#endif
