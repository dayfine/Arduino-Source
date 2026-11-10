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

//  The tiles a list of legs visits, starting with `from`. Throws
//  InternalProgramError if a leg is stopped before its end.
std::vector<Pokemon::TilePos> trace_legs(
    const StoryWorld& world,
    const Pokemon::TilePos& from,
    const std::vector<TileLeg>& legs
);




} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
