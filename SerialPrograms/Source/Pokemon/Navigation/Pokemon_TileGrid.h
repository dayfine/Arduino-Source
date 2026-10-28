/*  Tile Grid
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  An overworld map as a grid of tiles, and a planner that turns a walk from
 *  one tile to another into straight legs. Timed stick holds drift by a tile;
 *  a leg that ends against a wall does not, because the player stops there
 *  however long the stick is held. The planner prefers such legs.
 */

#ifndef PokemonAutomation_Pokemon_TileGrid_H
#define PokemonAutomation_Pokemon_TileGrid_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace Pokemon{


//  Legend of the ASCII rows a grid is parsed from.
enum class TileType{
    BLOCKED,        //  '#': trees, walls, items on the ground
    WALKABLE,       //  '.': open ground and paths
    TALL_GRASS,     //  '"': walkable, with wild encounters
    LEDGE,          //  'v': crossed only southward, hopping to the tile past it
    NPC,            //  'N': a standing NPC or trainer; blocks like a wall
    SIGN,           //  'S': a sign; blocks like a wall
    DOOR,           //  'D': walking into it enters a building
    UNKNOWN,        //  '?': off the map; an exit or ground we have no map of
};
char tile_char(TileType type);

//  Row 0 is the top of the map, so SOUTH increases y.
enum class GridDirection{
    NORTH,
    EAST,
    SOUTH,
    WEST,
};
const char* direction_name(GridDirection direction);

struct TilePos{
    int x = 0;
    int y = 0;

    TilePos next(GridDirection direction) const;
    bool operator==(const TilePos& other) const = default;
};
std::string to_string(const TilePos& pos);


class TileGrid{
public:
    //  Every row must have the same length and use only the legend's characters.
    //  Throws InternalProgramError otherwise.
    explicit TileGrid(const std::vector<std::string>& rows);

    //  The grid as the ASCII rows it parses from.
    std::vector<std::string> rows() const;

    int width() const{ return m_width; }
    int height() const{ return m_height; }
    bool in_bounds(const TilePos& pos) const;

    //  Out-of-bounds tiles read as UNKNOWN.
    TileType at(const TilePos& pos) const;

    //  Whether the player can stand on this tile.
    bool standable(const TilePos& pos) const;

    //  Where one step from `from` lands, or nothing if the player is stopped.
    //  A south step onto a ledge hops to the tile past it.
    std::optional<TilePos> step(const TilePos& from, GridDirection direction) const;

    //  Whether a step from `pos` is refused by a tile that holds the player
    //  in place. Doors and unknown tiles are not walls: walking into a door
    //  enters the building, and unknown ground may be an exit.
    bool at_wall(const TilePos& pos, GridDirection direction) const;

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<TileType> m_tiles;
};


//  Where the player stops when walking in one direction until stopped.
struct WallHit{
    TilePos stop;
    size_t tiles = 0;           //  tiles travelled; a ledge hop counts 2
    TilePos wall;               //  the first tile the player could not enter
    bool anchored = false;      //  the player is held at `stop` (see TileGrid::at_wall())
};
WallHit walk_to_wall(const TileGrid& grid, const TilePos& from, GridDirection direction);


//  A straight run in one direction.
struct TileLeg{
    GridDirection direction;
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
    const TileGrid& grid,
    const TilePos& from, const TilePos& to,
    const TilePlanCosts& costs = TilePlanCosts()
);

//  A trainer sees the tiles in a straight line in front of them, up to
//  `range` tiles or the first tile the player cannot stand on.
struct TrainerSight{
    std::string name;
    TilePos pos;
    GridDirection facing;
    int range = 4;
};
std::vector<TilePos> sight_tiles(const TileGrid& grid, const TrainerSight& trainer);

//  For each leg, the trainers whose sight line it enters.
std::vector<std::vector<std::string>> legs_crossing_sight(
    const TileGrid& grid,
    const TilePos& from,
    const std::vector<TileLeg>& legs,
    const std::vector<TrainerSight>& trainers
);

//  The tiles a list of legs visits, starting with `from`. Throws
//  InternalProgramError if a leg is stopped before its end.
std::vector<TilePos> trace_legs(
    const TileGrid& grid,
    const TilePos& from,
    const std::vector<TileLeg>& legs
);


//  A leg of a plan that allows for open legs going a tile short or long. A
//  leg to a wall is walked until the player stops; an open leg is walked for
//  `leg.tiles` tiles' worth of time. `after` lists every tile the player may
//  stand on once the leg is done.
struct PlannedLeg{
    TileLeg leg;
    std::vector<TilePos> after;
};
std::string to_string(const PlannedLeg& leg);

struct RobustPlanCosts : TilePlanCosts{
    int open_error = 1;         //  an open leg may end this many tiles short or long
    int max_open_tiles = 12;    //  longest open leg to consider
    size_t max_spread = 4;      //  most tiles the player may be on at once
};

//  Cheapest legs that take the player from any of the tiles in `from` to tiles
//  that are all `goal`, even if every open leg ends up to `open_error` tiles
//  off: the walls of later legs must bring the possible positions together.
//  Legs that would leave the map or enter a door are not used. Nothing if no
//  such plan exists.
std::optional<std::vector<PlannedLeg>> plan_robust_route(
    const TileGrid& grid,
    const std::vector<TilePos>& from, const std::function<bool(const TilePos&)>& goal,
    const RobustPlanCosts& costs = RobustPlanCosts()
);
//  The same, from one tile to exactly `to`.
std::optional<std::vector<PlannedLeg>> plan_robust_route(
    const TileGrid& grid,
    const TilePos& from, const TilePos& to,
    const RobustPlanCosts& costs = RobustPlanCosts()
);



}
}
#endif
