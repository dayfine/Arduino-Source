/*  Pokemon Center Room
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A Pokemon Center's ground floor, coded rather than mapped
 *  (PokemonFRLG_CodedRooms.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_PokemonCenterRoom_H
#define PokemonAutomation_PokemonFRLG_PokemonCenterRoom_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
