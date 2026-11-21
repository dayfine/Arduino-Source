/*  BDSP AutoStory - Route steps helpers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared low-level helpers used across PlannedRoute, PlannedTalk, and
 *  CountedWallLeg.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteSteps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteSteps_H

#include <cstddef>
#include <string>
#include "Common/Cpp/Time.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "PokemonBDSP_AutoStory_RouteIO.h"
#include "PokemonBDSP_AutoStory_RouteInput.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace detail{


//  A wait for someone walking about to get out of the way.
inline constexpr RouteInput WAIT_FOR_THE_WAY{
    .kind = RouteInputKind::WAIT,
    .direction = Pokemon::Direction::NORTH,
    .hold = Milliseconds(2500),
    .release = Milliseconds(0),
    .count = 1,
    .end = RouteInputEnd::DONE,
};

//  Long enough to reach the wall from the farthest tile, with room for a
//  slow start; past the wall the player stands still.
//  At a run the limit is still timed at a walk: a pace measured too fast
//  must not cut a long leg short.
Milliseconds wall_limit(const TileLeg& leg);

//  Fails the program with `message` (RouteNote::FAILURE).
[[noreturn]] void fail(RouteIO& io, const std::string& message);

//  route_turn() through `io`.
void turn(RouteIO& io, Pokemon::Direction direction);

//  A press of A, to talk to someone in front of us.
RouteInput press_a(RouteInputEnd end);

//  Tiles a walk to the wall moved, from the time it walked.
size_t tiles_moved(const WalkResult& walk, Milliseconds per_tile);


} // namespace detail
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
