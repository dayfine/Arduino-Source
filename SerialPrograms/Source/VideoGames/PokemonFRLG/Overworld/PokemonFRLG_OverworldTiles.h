/*  Overworld Tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A map's tiles as FRLG reads them: bounds, tile kinds, elevations,
 *  walls and water.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldTiles_H
#define PokemonAutomation_PokemonFRLG_OverworldTiles_H

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Direction enum from Pokemon navigation.
using Pokemon::Direction;
//  Grid tile coordinates.
using Pokemon::TilePos;
//  Classification of tile terrain.
using Pokemon::TileKind;
//  Parsed map representation.
using Pokemon::MapData;
//  Warp point connection.
using Pokemon::MapWarp;
//  Entity object on a map.
using Pokemon::MapObject;
//  Inter-map border connection.
using Pokemon::MapConnection;
//  Warp entry trigger definition.
using Pokemon::WarpEntry;
//  Finds warp located at specific tile coordinates.
using Pokemon::warp_at;


//  Elevations with a meaning of their own in the game's rules
//  (IsElevationMismatchAt() and ObjectEventUpdateElevation() in
//  pokefirered).
//  Stairs and ladders: they join every elevation.
constexpr uint8_t ELEVATION_ANY = 0;
//  Bridges and the like: they don't change the player's elevation.
constexpr uint8_t ELEVATION_IGNORED = 15;
//  Ordinary ground: most walkable tiles in the decompilation's map layouts.
//  Surf starts and ends here (CanStopSurfing(), and the Surf field move
//  checks the player's elevation is 3).
constexpr uint8_t ELEVATION_GROUND = 3;


//  How FRLG reads a map's tiles (Pokemon::TileKind) and warps:
//  - WALL, DOOR, SIGN and the ledges are walls. A DOOR warps the player who
//    walks north into it; a SIGN shows its message to the player who walks
//    into it moving north or south (signposts, Center and Mart signs;
//    pokefirered TrySetUpWalkIntoSignpostScript()). Ledges are hopped the
//    way they face, which isn't modelled, so the planner avoids them.
//  - WATER is entered only by starting to Surf.
//  - CLOSED_<side> can't be entered or left through that side.
//  - A warp entered by step_on (ladders, escalators) or cave_entrance warps
//    the player who steps onto its tile; walk_<direction> (exit mats, stairs
//    at the side of a room) warps the player standing on it who walks that
//    way.
//  A map without an elevation grid is all ground.

//  Whether the given point falls within the map bounds.
bool in_bounds(const MapData& map, const TilePos& point);
//  Out-of-bounds tiles are walls.
TileKind tile_at(const MapData& map, const TilePos& point);
//  Returns the elevation index at the given coordinate.
uint8_t elevation_at(const MapData& map, const TilePos& point);

//  Whether the tile at point is unwalkable wall terrain.
bool is_wall(const MapData& map, const TilePos& point);
//  Whether the tile at point is water terrain.
bool is_water(const MapData& map, const TilePos& point);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
