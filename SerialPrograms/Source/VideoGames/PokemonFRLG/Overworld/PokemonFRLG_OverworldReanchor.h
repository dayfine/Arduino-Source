/*  Overworld Re-anchor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Finding the player again after a battle or a message stopped a leg
 *  part-way. The timing of the stop narrows where they are to a few tiles of
 *  the leg (and a trainer who saw them may have stopped them earlier, on a
 *  tile in their sight); this looks for holds to walls, none to three, that
 *  end on the same tile from every one of them, and a way back to the leg's
 *  end that walks the same from there whichever it was. If there are none,
 *  the caller stops: the player is never guessed to be on one of them.
 *
 *  The player is taken to be on the leg's line. Wider sets were tried:
 *  with even one tile to either side, Route 1's people and ledges cut some
 *  hold for some tile, and nothing on its grass legs could be found.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldReanchor_H
#define PokemonAutomation_PokemonFRLG_OverworldReanchor_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How many tiles of a stopped leg the player may have walked: from `min`
//  to `max`, both included.
struct WalkedTiles{
    int min = 0;
    int max = 0;
};

//  How far off the timing of a stop may be: the interrupts' detection delays
//  aren't calibrated, and video lag and the watchers' period add to them.
constexpr Milliseconds STOP_TIMING_SLACK(300);

//  Where the timing puts a stop on `step`: `estimate` tiles walked
//  (Pokemon::LegResult::tiles), give or take STOP_TIMING_SLACK at
//  `tile_time` a tile: a tile either side walking, two running.
WalkedTiles walked_near(const RouteStep& step, int estimate, Milliseconds tile_time);
//  `route` with RouteStep::warp_slack set. An open leg over a map
//  connection is held a tile longer for the game holding the player up
//  there, which it doesn't always do: south from Route 6 into Vermilion
//  City it didn't, the player ended a tile further south, and the north 1
//  onto the Pokemon Center's door stopped below it (console B, 2026-11-03,
//  row 00.24). Until a leg to a wall that way or a warp places the player
//  again, they may stand a tile further on; a warp walked back that way is
//  held a tile further for it.
std::vector<RouteStep> with_warp_slack(std::vector<RouteStep> route);
//  Anywhere on `step`, for a stop the timing doesn't place.
WalkedTiles whole_leg(const RouteStep& step);
//  Where a stop on `step` leaves the player, from `timed`, the tiles its
//  timing gives: anywhere on the leg (whole_leg()) if it started on a dark
//  cave's screen (OverworldWalkSteps::dark_cave_screen()). There the timing
//  has no basis: the screen is black before any battle's wipe, and a
//  battle at the end of "east 12" on Rock Tunnel B1F was placed 0 to 3
//  tiles in (console B, 2026-11-09, 00.28). The player, "found again" at
//  a wall from those, was 5 tiles off.
WalkedTiles stop_tiles(const RouteStep& step, WalkedTiles timed, bool dark);

//  A trainer who saw the player and walked up to them: which of `map`'s
//  objects, and the tile next to the player they stand on afterwards.
struct TrainerStop{
    size_t object = 0;
    MapPosition at;

    bool operator==(const TrainerStop& other) const = default;
};

//  One way things may stand after a stop: the player's tile, and the
//  trainer who stopped them there, if one did.
struct StopCandidate{
    MapPosition player;
    std::optional<TrainerStop> trainer;

    bool operator==(const StopCandidate& other) const = default;
};
//  Formats a StopCandidate as a string.
std::string to_string(const StopCandidate& candidate);

//  Where `step`, an open WALK from `start`, may have been stopped:
//    - on the tiles `walked` covers, by a wild battle or a message;
//    - on any tile up to `walked.max` a trainer may see, by that trainer:
//      the player stops as soon as they are seen, and the trainer speaks
//      only after walking up, so later than the timing says.
//  Off the grass a message is taken to be a trainer's wherever one may see
//  the player, so there only the second kind are left, if there are any.
//  `start` not known: worked back from the leg's end. Empty if it can't be.
std::vector<StopCandidate> stop_candidates(
    const std::vector<MapData>& maps,
    const std::optional<MapPosition>& start,
    const RouteStep& step,
    WalkedTiles walked
);

//  A candidate reanchor route step with its cost estimate.
struct Reanchor{
    //  Legs to walls, in order, that bring the player to `found` from every
    //  candidate; none if there was only one. Each but the last ends on a
    //  tile that depends on the candidate; its `end` is one of them.
    std::vector<RouteStep> holds;
    MapPosition found;
    //  From `found` back to the stopped leg's end, walking the same from
    //  there whichever candidate it was (a trainer who walked up stays).
    std::vector<RouteStep> back;
};

//  How many tiles `anchor`'s holds and way back walk, as far as planned.
int reanchor_tiles(const Reanchor& anchor);

//  After `step` stopped part-way, from `candidates`: of the ways that find
//  the player, the one that walks the fewest tiles, holds and way back
//  together, with tiles of open legs weighted over held ones. Nothing if
//  no holds bring them all to one tile, or no way back walks the same from
//  there.
std::optional<Reanchor> plan_reanchor(
    const std::vector<MapData>& maps,
    const std::vector<StopCandidate>& candidates,
    const RouteStep& step,
    const RouteCosts& costs = default_route_costs()
);

//  Walking on along a stopped open leg instead of finding the player at
//  walls first: the rest of the leg from the middle of the tiles they may
//  be on, after which the route's next steps, as planned, end with a hold
//  that brings them to one tile from every one of them. Walking on from
//  how far a leg got, trusting the timing, went wrong on the console; here
//  a hold finds the player whichever tile they were on.
struct Resume{
    //  From the middle candidate to the stopped leg's end; no tiles if the
    //  middle candidate is its end.
    RouteStep rest;
    //  How many of the steps after the stopped leg go by until one, a hold
    //  to a wall, brings every candidate to one tile.
    size_t steps = 0;
    //  Where each of those steps may start, `rest` first.
    std::vector<std::vector<MapPosition>> starts;
};

//  After `step`, an open leg, stopped part-way on one of `candidates`
//  (none stopped by a trainer), with `after` the route's steps after it:
//  the way to walk on. Nothing if, from some candidate, a step of `after`
//  before a hold that brings them all to one tile would walk otherwise
//  than planned, or a way would cross a tile off the planned way that a
//  trainer may see.
std::optional<Resume> plan_resume(
    const std::vector<MapData>& maps,
    const std::vector<StopCandidate>& candidates,
    const RouteStep& step,
    const std::vector<RouteStep>& after
);

//  Holds to walls that bring the player to one tile from every one of
//  `tiles`, found as plan_reanchor() finds them, with no trainer about: for
//  a player known to be on one of a few tiles without a stop (a wild
//  battle's pacing). Empty if `tiles` are one tile; the last hold ends on
//  it otherwise. Nothing if no holds bring them together.
std::optional<std::vector<RouteStep>> holds_to_one_tile(
    const std::vector<MapData>& maps, const std::vector<MapPosition>& tiles
);


//  Registers overworld reanchor unit tests.
void add_tests_OverworldReanchor(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
