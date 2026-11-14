/*  Overworld Tile Rules
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The walking rules of FRLG's special tiles: grass, ledges, one-way
 *  edges, warps, doors and exit mats.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldTileRules_H
#define PokemonAutomation_PokemonFRLG_OverworldTileRules_H

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "PokemonFRLG_OverworldTiles.h"
#include "PokemonFRLG_OverworldWildTiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether the tile at point is a one-way ledge.
bool is_ledge(const MapData& map, const TilePos& point);
//  Whether the tile itself refuses a step in `direction`: entering it, or
//  leaving it.
bool blocks_entering(const MapData& map, const TilePos& point, Direction direction);
//  Whether leaving the tile in the given direction is blocked.
bool blocks_leaving(const MapData& map, const TilePos& point, Direction direction);

//  Whether the game takes this tile's warp on stepping onto it (a ladder or
//  cave entrance).
bool warps_on_step(const MapData& map, const TilePos& point);
//  A door, entered by walking north into it.
bool is_door(const MapData& map, const TilePos& point);
//  The direction that takes the exit mat on this tile, if it is one.
std::optional<Direction> exit_mat(const MapData& map, const TilePos& point);
//  Arriving on a door or cave entrance, the game walks the player one
//  tile south, out of it.
bool walks_out_on_arrival(const MapData& map, const TilePos& point);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
