/*  AutoStory Room Legs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The scripted legs through a Pokemon Center and the Rocket Hideout's
 *  lift.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRoomLegs_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRoomLegs_H

#include <string_view>
#include <vector>
#include "PokemonFRLG_AutoStoryRooms.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Into a Center: north through the door, north 4 to the nurse.
const std::vector<RoomLeg>& center_enter_legs();

//  Out of a Center: south 4 to the mat, south through the door.
const std::vector<RoomLeg>& center_exit_legs();

//  Into the hideout's lift, from below a floor's lift door facing north:
//  north through the door onto LIFT_ARRIVAL, north 2, west 2 to below the
//  panel (LIFT_PANEL_FRONT), facing it.
const std::vector<RoomLeg>& lift_in_legs();

//  Out of the lift, from below the panel: east 2, south 3 onto the arrow
//  warp and through it, onto the floor picked's lift door. On the console
//  the second step may take the warp; the third then walks into the wall
//  below the door. The fade only dims the screen (RoomLeg::dims).
const std::vector<RoomLeg>& lift_out_legs();

//  The legs of the Center's step `variant`; empty if it isn't one.
const std::vector<RoomLeg>& center_legs(std::string_view variant);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
