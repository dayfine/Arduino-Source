/*  BDSP AutoStory - Wall walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A walk to a wall, turned the way first.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WallWalk_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WallWalk_H

#include <optional>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_RouteIO.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  io.walk_to_wall(), with a leading turn not counted as a moved chunk:
//  facing another way (`facing` known), a tap that only turns comes first, so
//  the walk counts from its first step. Facing unknown (after a battle), the
//  walk's first chunk that changed the screen may be the turn: it is not
//  counted when the walk ends at the wall.
WalkResult walk_to_wall_turning(RouteIO& io, std::optional<Pokemon::Direction> facing, const RouteWallWalk& walk);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
