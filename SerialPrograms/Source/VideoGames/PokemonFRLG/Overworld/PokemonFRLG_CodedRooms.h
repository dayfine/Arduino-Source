/*  Coded Rooms and Tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Places the story scripts rather than maps (the user's call, 2026-11-09):
 *  every Pokemon Center's ground floor, which all share one layout, the
 *  Rocket Hideout's lift, a room of 5 by 7 tiles, and the tiles that maps
 *  can't say in glyphs: the Rocket Hideout's spinners and the Game
 *  Corner's poster. All of it is read off pret/pokefirered, cited where it
 *  is defined, so no map file has to carry it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_CodedRooms_H
#define PokemonAutomation_PokemonFRLG_CodedRooms_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The spinner arrow on `point` of the map named `map`, from the
//  decompilation's metatile behaviours, if the map is one of the hideout's
//  spinner floors (RocketHideout_B2F, RocketHideout_B3F).
std::optional<Direction> coded_spin_direction(std::string_view map, const TilePos& point);
//  Whether `point` of the map named `map` is a coded MB_STOP_SPINNING tile.
bool coded_spin_stop(std::string_view map, const TilePos& point);


//  The Game Corner, whose poster hides the hideout's switch.
inline constexpr std::string_view GAME_CORNER_MAP = "CeladonCity_GameCorner";
//  The poster (pokefirered CeladonCity_GameCorner map.json, the bg event
//  CeladonCity_GameCorner_EventScript_Poster at (11, 1)).
inline constexpr TilePos GAME_CORNER_POSTER{11, 1};
//  Adds what the Game Corner's map can't say to `map`, if it is the Game
//  Corner and hasn't them: the poster, an object "poster", and the tiles
//  the switch turns into stairs (CeladonCity_GameCorner_EventScript_
//  OpenRocketHideout's setmetatile: (15, 2), (16..17, 2..3)), objects
//  "poster_stairs" that block them until it is pushed.
void add_coded_objects(MapData& map);


//  A Pokemon Center's ground floor. pokefirered gives all of Kanto's and
//  the Sevii Islands' but the Indigo Plateau's and One Island's the one
//  layout, LAYOUT_POKEMON_CENTER_1F; the seven in the map lock have the
//  same tiles and differ only in who stands where, none of them on the
//  nurse's column.
//  The mat inside the door: the player arrives on it and leaves from it,
//  walking south.
inline constexpr TilePos CENTER_MAT{7, 8};
//  Below the nurse's counter, facing her (the nurse at (7, 2)).
inline constexpr TilePos CENTER_NURSE_FRONT{7, 4};
//  Whether `map` names a Pokemon Center's ground floor.
bool is_pokemon_center(std::string_view map);
//  The ground floor of the Center named `name`, whose mat lets out on
//  `outside` (the door tile of the town's map): its tiles, the nurse, and
//  the mat's warps. No person but the nurse: who stands where differs, and
//  the script keeps to the nurse's column.
MapData pokemon_center_room(const std::string& name, const Pokemon::WarpDestination& outside);
//  For a sim: adds the coded room of the Center whose door is on
//  `maps[town]` to `maps`, and returns its name; nothing if the town has
//  no Center door. The town gets a point "<Center>_door" on its door tile
//  for the room's mat to let out on.
std::optional<std::string> add_pokemon_center_room(std::vector<MapData>& maps, size_t town);


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
