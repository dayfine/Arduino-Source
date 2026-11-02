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
WallHit slide_to_wall(const StoryWorld& world, const Pokemon::TilePos& from, Pokemon::Direction direction);


//  A straight run in one direction.
struct TileLeg{
    Pokemon::Direction direction = Pokemon::Direction::NORTH;
    size_t tiles = 0;           //  tiles travelled; a ledge hop counts 2
    bool ends_at_wall = false;  //  the next tile in `direction` stops the player
};
std::string to_string(const TileLeg& leg);

struct TilePlanCosts{
    int per_tile = 1;
    int per_grass_tile = 4;     //  extra for each tall-grass tile entered
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
std::string to_string(const PlannedLeg& leg);

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
//  Legs that would leave the maps or enter a door are not used. Nothing if no
//  such plan exists, or none is found within `costs.max_searched` sets.
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



}
}
}
#endif
