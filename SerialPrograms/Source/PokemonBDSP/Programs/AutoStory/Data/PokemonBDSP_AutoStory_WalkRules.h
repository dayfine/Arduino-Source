/*  BDSP AutoStory - Walk rules
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the player can stand and step on a story world, from its tiles.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WalkRules_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WalkRules_H

#include <cstddef>
#include <optional>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Whether a map holds this world tile.
bool covered(const StoryWorld& world, const Pokemon::TilePos& pos);
//  Whether the player can stand on this tile.
bool standable(const StoryWorld& world, const Pokemon::TilePos& pos);
//  Where one step from `from` lands, or nothing if the player is stopped.
//  A step onto a ledge the arrow's way hops to the tile past it.
std::optional<Pokemon::TilePos> step(const StoryWorld& world, const Pokemon::TilePos& from, Pokemon::Direction direction);
//  Whether a step from `pos` is refused by a tile that holds the player
//  in place. Doors and tiles off the maps are not walls: walking into a
//  door enters the building, and ground off the maps may be an exit.
bool at_wall(const StoryWorld& world, const Pokemon::TilePos& pos, Pokemon::Direction direction);
//  The tiles the player passes when walking `tiles` tiles from `from`, the
//  first one after `from` first. Stops early where the player is stopped.
std::vector<Pokemon::TilePos> leg_path(const StoryWorld& grid, Pokemon::TilePos from, Pokemon::Direction direction, size_t tiles);
//  Whether a warp (a door's tile) lies within `reach` tiles `direction` of
//  one of `from`, nothing stopping the player before it: where a walk that
//  ran on past its leg's end may have gone through (#268 slice 2).
bool warp_within(const StoryWorld& world, const std::vector<Pokemon::TilePos>& from, Pokemon::Direction direction, size_t reach);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
