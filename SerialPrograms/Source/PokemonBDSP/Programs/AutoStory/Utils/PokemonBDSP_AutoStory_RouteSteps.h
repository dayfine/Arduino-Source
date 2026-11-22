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
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
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

//  Sorted by row, then column, without repeats.
std::vector<Pokemon::TilePos> sorted_unique_tiles(std::vector<Pokemon::TilePos> tiles);

//  Tiles where someone wanders on the way from any tile in `from` to the wall,
//  or one row/column beside it.
std::vector<Pokemon::TilePos> wanderers_along_wall_leg(
    const StoryWorld& grid,
    const std::vector<Pokemon::TilePos>& from,
    Pokemon::Direction direction
);

//  Tiles one step behind `zone` in the opposite direction of `ahead`.
std::vector<Pokemon::TilePos> tiles_before_wander_zone(
    const StoryWorld& grid,
    const std::vector<Pokemon::TilePos>& zone,
    Pokemon::Direction ahead
);

//  Result of tapping back to the wall behind an early stop.
struct BackToWallResult{
    std::vector<Pokemon::TilePos> tiles;
    //  Non-empty when the taps uniquely identified the single stopped tile.
    std::vector<Pokemon::TilePos> identified_stop;
};

//  Taps back to the wall behind `stopped` to narrow down which tile stopped
//  the walk. Returns empty `tiles` if no safe wall lies behind `stopped`.
BackToWallResult tap_back_to_wall(
    const StoryWorld& grid,
    const std::vector<Pokemon::TilePos>& stopped,
    Pokemon::Direction ahead,
    const std::string& label,
    RouteIO& io
);


} // namespace detail
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
