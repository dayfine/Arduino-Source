/*  BDSP AutoStory - Robust route planner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plans walks whose open legs may go a tile short or long: each leg
 *  lists every tile the player may end on, and later walls bring them
 *  back together.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RobustRoute_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RobustRoute_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A leg of a plan that allows for open legs going a tile short or long. A
//  leg to a wall is walked until the player stops; an open leg is walked for
//  `leg.tiles` tiles' worth of time. `after` lists every tile the player may
//  stand on once the leg is done.
struct PlannedLeg{
    TileLeg leg;
    std::vector<Pokemon::TilePos> after;
};
//  Formats planned route leg with uncertainties into human-readable representation.
std::string to_string(const PlannedLeg& leg);

//  Cost configuration parameters accounting for movement execution tolerances.
struct RobustPlanCosts : TilePlanCosts{
    //  An open leg of n tiles may end open_error + n / open_error_per tiles
    //  short or long: on the console 7 tiles went 9.
    int open_error = 1;
    int open_error_per = 5;
    //  Open legs this many tiles or shorter land exactly: a short hold moves
    //  whole tiles. 0 keeps every open leg uncertain.
    int exact_open_tiles = 0;
    //  Extra per tile of an open leg: its timing error grows with its length,
    //  so a short step after a wall beats a long open leg.
    int per_open_tile = 0;
    int max_open_tiles = 12;    //  longest open leg to consider
    size_t max_spread = 5;      //  most tiles the player may be on after a leg
    //  Most sets of possible tiles to search before giving up. The search is
    //  over sets, not tiles, so it grows far faster than the map: all of
    //  Jubilife City from six start tiles ran for minutes with no plan
    //  (r224a, 2026-10-31); 200000 sets took 31 s there, so this gives up
    //  after about 3. Use town costs in towns (a whole-Jubilife plan then
    //  takes 1 s) and crop the map to the part a leg needs rather than
    //  raising this.
    size_t max_searched = 20000;
};

//  Cheapest legs that take the player from any of the tiles in `from` to tiles
//  that are all `goal`, even if every open leg ends up to `open_error` tiles
//  off: the walls of later legs must bring the possible positions together.
//  Legs that would leave the maps or enter a door are not used, nor legs that
//  run along a zone where someone wanders: two tiles of it in a row, the
//  leg's start among them. Crossing a zone (one tile of it) costs
//  `per_wander_tile`. On lab-B (2026-11-08, 01.08) Route 204's youngster,
//  who wanders (12, 2) to (14, 2), stopped the leg "west 2 (to wall)" from
//  (13, 2) twice. Nothing if no such plan exists, or none is found within
//  `costs.max_searched` sets.
std::optional<std::vector<PlannedLeg>> plan_robust_route(
    const StoryWorld& world,
    const std::vector<Pokemon::TilePos>& from, const std::function<bool(const Pokemon::TilePos&)>& goal,
    const RobustPlanCosts& costs = RobustPlanCosts()
);
//  The same, from one tile to exactly `to`.
std::optional<std::vector<PlannedLeg>> plan_robust_route(
    const StoryWorld& world,
    const Pokemon::TilePos& from, const Pokemon::TilePos& to,
    const RobustPlanCosts& costs = RobustPlanCosts()
);

//  What plan_robust_route() charges for walking `legs` from any of `from`,
//  each leg from the tiles the one before may end on. A leg it would not
//  plan (along a wander zone, or off the maps) costs its tiles and legs
//  without grass or wander tiles.
long long robust_route_cost(
    const StoryWorld& world,
    const std::vector<Pokemon::TilePos>& from, const std::vector<PlannedLeg>& legs,
    const RobustPlanCosts& costs
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
