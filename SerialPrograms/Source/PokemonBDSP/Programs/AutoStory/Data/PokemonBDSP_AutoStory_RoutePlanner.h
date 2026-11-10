/*  BDSP AutoStory - Route planner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Turns a walk on the story world from one tile to another into straight
 *  legs. Timed stick holds drift by a tile; a leg that ends against a wall
 *  does not, because the player stops there however long the stick is held.
 *  The planner prefers such legs.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RoutePlanner_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RoutePlanner_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Where the player stops when walking in one direction until stopped.
struct WallHit{
    Pokemon::TilePos stop;
    size_t tiles = 0;           //  tiles travelled; a ledge hop counts 2
    Pokemon::TilePos wall;      //  the first tile the player could not enter
    bool anchored = false;      //  the player is held at `stop` (see StoryWorld::at_wall())
};
//  Computes straight-line sliding movement from tile position until hitting a wall.
WallHit slide_to_wall(const StoryWorld& world, const Pokemon::TilePos& from, Pokemon::Direction direction);


//  A straight run in one direction.
struct TileLeg{
    Pokemon::Direction direction = Pokemon::Direction::NORTH;
    size_t tiles = 0;           //  tiles travelled; a ledge hop counts 2
    bool ends_at_wall = false;  //  the next tile in `direction` stops the player
    //  An open leg held for its tiles' time instead of tapped a tile at a
    //  time: much faster, but it may end Pokemon::open_hold_error() tiles
    //  short or long (RobustPlanCosts::hold_open_legs).
    bool held = false;
};
//  Formats tile movement leg into human-readable string representation.
std::string to_string(const TileLeg& leg);

//  Cost configuration parameters for tile route planning.
struct TilePlanCosts{
    int per_tile = 1;
    int per_grass_tile = 4;     //  extra for each tall-grass tile entered
    int per_wander_tile = 3;    //  extra for each wander hazard tile entered
    int per_leg = 8;
    int leg_not_at_wall = 50;   //  extra for a leg that stops in the open
};

//  Cheapest legs from `from` to `to`, moving 4 ways, or nothing if `to`
//  cannot be reached. Both tiles must be standable.
std::optional<std::vector<TileLeg>> plan_tile_route(
    const StoryWorld& world,
    const Pokemon::TilePos& from, const Pokemon::TilePos& to,
    const TilePlanCosts& costs = TilePlanCosts()
);

//  A trainer sees the tiles in a straight line in front of them, up to
//  `range` tiles or the first tile the player cannot stand on. The maps
//  have no sight ranges, so story code lists them.
struct TrainerSight{
    std::string name;
    Pokemon::TilePos pos;
    Pokemon::Direction facing = Pokemon::Direction::NORTH;
    int range = 4;
};
//  Computes straight line-of-sight tiles for a trainer.
std::vector<Pokemon::TilePos> sight_tiles(const StoryWorld& world, const TrainerSight& trainer);

//  For each leg, the trainers whose sight line it enters.
std::vector<std::vector<std::string>> legs_crossing_sight(
    const StoryWorld& world,
    const Pokemon::TilePos& from,
    const std::vector<TileLeg>& legs,
    const std::vector<TrainerSight>& trainers
);

//  The tiles a list of legs visits, starting with `from`. Throws
//  InternalProgramError if a leg is stopped before its end.
std::vector<Pokemon::TilePos> trace_legs(
    const StoryWorld& world,
    const Pokemon::TilePos& from,
    const std::vector<TileLeg>& legs
);


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
    //  An open leg of n tiles may end Pokemon::open_hold_error(n) tiles
    //  short or long: on the console 7 tiles went 9.
    //  Open legs this many tiles or shorter land exactly: a short hold moves
    //  whole tiles. 0 keeps every open leg uncertain.
    int exact_open_tiles = 0;
    //  Extra per tile of an open leg: its timing error grows with its length,
    //  so a short step after a wall beats a long open leg.
    int per_open_tile = 0;
    int max_open_tiles = 12;    //  longest open leg to consider
    //  Open legs longer than exact_open_tiles are held for their time
    //  (TileLeg::held), not tapped: no leg is planned whose possible ends
    //  past its shortest cross tall grass, a wanderer's zone or a ledge, so
    //  a long or short hold meets nothing a tap would not have. Off, such
    //  legs are planned as uncertain but walked in taps.
    bool hold_open_legs = false;
    //  Extra per tall-grass tile an open leg enters. A wild battle on a leg
    //  to a wall costs nothing to recover from: RouteWalker walks to the
    //  wall again. On an open leg it loses the tile count, and the walk
    //  plans again from the tiles round it, often back through the same
    //  grass (00.04b to_catch_grass, 00.04d north_to_jubilife and 01.06
    //  up_jubilifes_road were scored STRAND for that).
    int per_open_grass_tile = 0;
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
//  that are all `goal`, even if every open leg ends up Pokemon::open_hold_error() tiles
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
