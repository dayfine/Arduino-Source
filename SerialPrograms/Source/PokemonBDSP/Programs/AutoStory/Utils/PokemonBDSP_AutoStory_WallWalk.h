/*  BDSP AutoStory - Wall walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A walk to a wall, turned the way first; a walk on to it after a battle.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WallWalk_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WallWalk_H

#include <optional>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_PlannedRoute.h"
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

//  The leg to a wall before the one walked, to walk on to again if a stop on
//  it may not have been its wall.
struct WallBefore{
    Pokemon::Direction direction;
    Milliseconds limit;
};

//  A walk on to the wall after a trainer's battle on the way, fighting any
//  trainer who froze the walk a tile short of the wall: that stop reads as
//  the wall, and the dialogue opens after it (lab-B 2026-11-12 17:46, 01.02:
//  Route 203's corner trainer). `began_still`: the battle began before the
//  leg moved. After a `before` leg to a wall, that stop may have been such a
//  freeze, and the column or row we stand on is then not where the plan has
//  it: that wall comes first.
WalkResult walk_on_to_wall(
    RouteIO& io, RouteTrainers& trainers,
    const std::optional<WallBefore>& before, bool began_still,
    const RouteWallWalk& walk
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
