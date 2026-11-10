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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
