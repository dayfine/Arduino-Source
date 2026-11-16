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
//  floor picked (SimScript::lift_floors with rocket_hideout_lift_exits()).
MapData rocket_hideout_lift_room();
//  Where the lift lets out for each floor on its panel, top first
//  (setdynamicwarp in RocketHideout_Elevator's scripts): the warp lands on
//  the floor's lift door, B1F (24, 25), B2F (28, 16), B4F (20, 23), and
//  the game walks the player a tile south off it, onto (24, 26), (28, 17)
//  and (20, 24) (console B, 2026-11-11, B4F).
const std::vector<MapPosition>& rocket_hideout_lift_exits();

//  The Celadon Department Store's lift: the same layout as the hideout's
//  (LAYOUT_CELADON_CITY_DEPARTMENT_STORE_ELEVATOR's map.bin is byte for
//  byte LAYOUT_ROCKET_HIDEOUT_ELEVATOR's), so the same LIFT_ARRIVAL and
//  LIFT_PANEL_FRONT. Its floors' lift doors lead to its warp 0, (2, 5).
inline constexpr std::string_view DEPT_STORE_LIFT_MAP = "CeladonCity_DepartmentStore_Elevator";
//  The store lift's room, under DEPT_STORE_LIFT_MAP. Its door is a dynamic
//  warp, as the hideout's (SimScript::lift_floors with
//  department_store_lift_exits()).
MapData department_store_lift_room();
//  Where it lets out for each floor on its panel, top first: every floor's
//  lift door is (6, 1) (setdynamicwarp ..., 255, 6, 1 in its scripts), and
//  the player is walked a tile south off it, onto (6, 2).
const std::vector<MapPosition>& department_store_lift_exits();


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
