/*  Overworld Map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  An FRLG map as the game sees it: per tile collision, elevation and the
 *  behaviors that matter for walking (water, one-way edges), plus its warps
 *  and the objects standing on it. The map data is generated from the
 *  pret/pokefirered decompilation, and classify_step() follows the game's own
 *  collision checks (GetCollisionAtCoords() in src/event_object_movement.c),
 *  so a route planned on these maps is one the game allows.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldMap_H
#define PokemonAutomation_PokemonFRLG_OverworldMap_H

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using Pokemon::Direction;
using Pokemon::TilePos;


//  A warp from (x, y) to warp `dest_warp` of `dest_map`. Whether and how the
//  player takes it depends on the tile there (see MapData::tiles). An empty
//  `dest_map` leads to a map that isn't loaded.
struct MapWarp{
    int x = 0;
    int y = 0;
    std::string dest_map;
    int dest_warp = 0;
};

//  An object standing on a tile (person, item ball, rock, Pokemon), which
//  blocks it. `kind` is its graphics, e.g. "nurse" or "mewtwo". The game
//  hides it once its event flag is set, e.g. a guard who leaves after the
//  Hall of Fame; an empty flag means it is always there. A person who wanders
//  may stand anywhere up to `wander_x` and `wander_y` tiles from (x, y), so
//  all of those tiles count as blocked.
struct MapObject{
    int x = 0;
    int y = 0;
    std::string kind;
    std::string flag;
    int wander_x = 0;
    int wander_y = 0;
};

//  Walking off the map's `direction` edge continues on `map`, whose tiles are
//  shifted by `offset` along that edge (the decompilation's map connections).
struct MapConnection{
    Direction direction = Direction::NORTH;
    int offset = 0;
    std::string map;
};

//  A map as generated from the decompilation (see PokemonFRLG_OverworldMaps.h
//  for its JSON form).
struct MapData{
    std::string name;
    int width = 0;
    int height = 0;
    //  `height` rows of `width` characters, row 0 at the top:
    //    '#'  collision (wall, cliff)
    //    'J'  ledge: a wall, except that walking into it the way it faces
    //         hops the player down (not modelled, so the planner avoids it)
    //    '.'  floor
    //    '"'  tall grass: floor with wild encounters
    //    '~'  water: entered only by starting to Surf
    //    'D'  door: a wall that warps the player who walks north into it
    //    'C'  cave entrance: floor that warps the player who steps onto it
    //    'L'  ladder or escalator: floor that warps the player who steps onto it
    //    '^' 'v' '<' '>'  exit mat: floor that warps the player standing on it
    //         who walks in the arrow's direction
    //    'n'  can't be entered moving south or left moving north
    //    's'  can't be entered moving north or left moving south
    //    'e'  can't be entered moving west or left moving east
    //    'w'  can't be entered moving east or left moving west
    std::vector<std::string> tiles;
    //  One hex digit per tile. Walking keeps to one elevation; tiles of
    //  elevation 0 (stairs, ladders) join elevations, and 15 ignores them.
    std::vector<std::string> elevation;
    std::vector<MapWarp> warps;
    std::vector<MapObject> objects;
    std::vector<MapConnection> connections;
};

//  Throws ParseException, naming the map, if the rows don't match its size
//  or use a character outside the legend.
void check_map(const MapData& map);

bool in_bounds(const MapData& map, const TilePos& point);
//  The tile's character (see MapData::tiles). Out-of-bounds tiles are walls.
char terrain_at(const MapData& map, const TilePos& point);
uint8_t elevation_at(const MapData& map, const TilePos& point);

bool is_wall(const MapData& map, const TilePos& point);
bool is_water(const MapData& map, const TilePos& point);
bool is_tall_grass(const MapData& map, const TilePos& point);
bool is_ledge(const MapData& map, const TilePos& point);
//  Whether the tile itself refuses a step in `direction`: entering it, or
//  leaving it.
bool blocks_entering(const MapData& map, const TilePos& point, Direction direction);
bool blocks_leaving(const MapData& map, const TilePos& point, Direction direction);

//  The warp the game takes on stepping onto this tile ('C', 'L').
bool warps_on_step(const MapData& map, const TilePos& point);
//  A door ('D'), entered by walking north into it.
bool is_door(const MapData& map, const TilePos& point);
//  The direction that takes the exit mat on this tile, if it is one.
std::optional<Direction> exit_mat(const MapData& map, const TilePos& point);
//  Arriving on a door or cave entrance, the game walks the player one
//  tile south, out of it.
bool walks_out_on_arrival(const MapData& map, const TilePos& point);

const MapObject* object_at(const MapData& map, const TilePos& point);
const MapWarp* warp_at(const MapData& map, const TilePos& point);

//  Removes the objects the game hides once one of `flags` is set.
void hide_objects(MapData& map, const std::set<std::string>& flags);


//  What decides where the player's next step can go.
struct PlayerState{
    TilePos position;
    uint8_t elevation = 3;
    bool surfing = false;
};

enum class StepKind{
    BLOCKED,
    WALK,           //  moves one tile (on land, or on water while surfing)
    STOP_SURFING,   //  moves from water onto land and stops surfing
    START_SURF,     //  blocked, but facing water the player can Surf onto
};

//  What pressing `direction` once does to the player in `state`.
StepKind classify_step(const MapData& map, const PlayerState& state, Direction direction);

//  The same for a step that crosses into another map: from `state` on `from`
//  onto `destination` on `to`.
StepKind classify_step_onto(
    const MapData& from, const PlayerState& state, Direction direction,
    const MapData& to, const TilePos& destination
);

//  The player after a step of kind WALK, STOP_SURFING or START_SURF (the
//  Surf action moves the player onto the water).
PlayerState after_step(const MapData& map, const PlayerState& state, Direction direction, StepKind kind);
PlayerState after_step_onto(
    const MapData& from, const PlayerState& state,
    const MapData& to, const TilePos& destination, StepKind kind
);



}
}
}
#endif
