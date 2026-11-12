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

//  Into a lift (the hideout's or the store's), from below a floor's lift door facing north:
//  north through the door onto LIFT_ARRIVAL, north 2, west 2 to below the
//  panel (LIFT_PANEL_FRONT), facing it.
const std::vector<RoomLeg>& lift_in_legs();

//  Out of the lift, from anywhere on its floor (rows 3 to 5): west 4 and
//  south 2 into its corner (0, 5), east 2 onto the arrow warp, south
//  through it, out a tile south of the floor picked's lift door. The
//  corner absorbs taps a ride's shake swallows. The warp's fade only dims
//  the screen (RoomLeg::dims; console B, 2026-11-11, 09.02 runs 5 and 6:
//  a black-fade wait missed it and its retries walked on into B4F).
const std::vector<RoomLeg>& lift_out_legs();

//  The legs of the Center's step `variant`; empty if it isn't one.
const std::vector<RoomLeg>& center_legs(std::string_view variant);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
