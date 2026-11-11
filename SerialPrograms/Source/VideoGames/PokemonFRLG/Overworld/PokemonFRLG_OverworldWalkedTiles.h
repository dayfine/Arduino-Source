/*  Overworld Walked Tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How many tiles of a stopped leg the player may have walked, from the
 *  stop's timing or, when it doesn't place the stop, the whole leg.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalkedTiles_H
#define PokemonAutomation_PokemonFRLG_OverworldWalkedTiles_H

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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
