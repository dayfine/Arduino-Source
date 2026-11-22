/*  Mail Glitch Coded Rooms
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The five indoor rooms the Mail Glitch Activation program enters whose map
 *  files are not in the AutoStory map pack: Celadon City's Restaurant, Game
 *  Corner Prize Room, Department Store 4F, Vermilion City's House 2 (CH'DING),
 *  and Route 2's House (MIMIEN). Layouts and warps are read directly from
 *  pret/pokefirered.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MailGlitchRooms_H
#define PokemonAutomation_PokemonFRLG_MailGlitchRooms_H

#include <vector>
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Adds the five coded indoor rooms for Mail Glitch Activation to `maps` if
//  not already present. Each room's exit warps link back to the corresponding
//  outdoor map's warp point.
void add_mail_glitch_rooms(std::vector<MapData>& maps);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
