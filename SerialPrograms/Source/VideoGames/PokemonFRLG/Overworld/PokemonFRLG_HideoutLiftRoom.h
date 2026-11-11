/*  Hideout Lift Room
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Rocket Hideout's lift, coded rather than mapped
 *  (PokemonFRLG_CodedRooms.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_HideoutLiftRoom_H
#define PokemonAutomation_PokemonFRLG_HideoutLiftRoom_H

#include <vector>
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The Rocket Hideout's lift (pokefirered LAYOUT_ROCKET_HIDEOUT_ELEVATOR,
//  5 by 7 tiles, rows 3 to 5 walkable).
inline constexpr std::string_view LIFT_MAP = "RocketHideout_Elevator";
//  Where the floors' lift doors put the player (warp 1, an
//  MB_SOUTH_ARROW_WARP tile): south from it leaves the lift.
inline constexpr TilePos LIFT_ARRIVAL{2, 5};
//  Below the panel (a bg event at (0, 2), RocketHideout_Elevator_
//  EventScript_FloorSelect), facing north.
inline constexpr TilePos LIFT_PANEL_FRONT{0, 3};
//  The lift's room. Its door is a dynamic warp: the sim takes it to the
//  floor picked (SimScript::lift_floors with rocket_hideout_lift_doors()).
MapData rocket_hideout_lift_room();
//  Where the lift lets out for each floor on its panel, top first
//  (setdynamicwarp in RocketHideout_Elevator's scripts): B1F (24, 25),
//  B2F (28, 16), B4F (20, 23), each its floor's lift door.
const std::vector<MapPosition>& rocket_hideout_lift_doors();


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
