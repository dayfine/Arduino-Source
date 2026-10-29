/*  Overworld Map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  An FRLG map as the game sees it: per tile collision, elevation and the
 *  behaviors that matter for walking (water, one-way edges), plus its warps
 *  and the objects standing on it. The map data is generated from the
 *  pret/pokefirered decompilation, and try_step() follows the game's own
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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


enum class MapDirection{
    NORTH,
    EAST,
    SOUTH,
    WEST,
};
const char* direction_name(MapDirection direction);


//  Tile coordinates. Row 0 is the top of the map, so SOUTH increases y.
struct MapPoint{
    int x = 0;
    int y = 0;

    MapPoint next(MapDirection direction) const;
    bool operator==(const MapPoint& other) const = default;
};
std::string to_string(const MapPoint& point);


//  A warp from (x, y) to warp `dest_warp` of `dest_map`. Whether and how the
//  player takes it depends on the tile there (see MapText::terrain). An empty
//  `dest_map` leads to a map that isn't loaded.
struct MapWarp{
    int x = 0;
    int y = 0;
    std::string dest_map;
    int dest_warp = 0;
};

//  An object standing on a tile (person, item ball, rock, Pokemon), which
//  blocks it. The game hides it once its event flag is set, e.g. a guard who
//  leaves after the Hall of Fame; an empty flag means it is always there. A
//  person who wanders may stand anywhere up to `wander_x` and `wander_y` tiles
//  from (x, y), so all of those tiles count as blocked.
struct MapObject{
    int x = 0;
    int y = 0;
    std::string name;
    std::string flag;
    int wander_x = 0;
    int wander_y = 0;
};

//  Walking off the map's `direction` edge continues on `map`, whose tiles are
//  shifted by `offset` along that edge (the decompilation's map connections).
struct MapConnection{
    MapDirection direction = MapDirection::NORTH;
    int offset = 0;
    std::string map;
};

//  A map as generated from the decompilation.
struct MapText{
    std::string name;
    //  One character per tile:
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
    std::vector<std::string> terrain;
    //  One hex digit per tile. Walking keeps to one elevation; tiles of
    //  elevation 0 (stairs, ladders) join elevations, and 15 ignores them.
    std::vector<std::string> elevation;
    std::vector<MapWarp> warps;
    std::vector<MapObject> objects;
    std::vector<MapConnection> connections;
};


class OverworldMap{
public:
    //  Throws InternalProgramError if the rows don't match in size or use a
    //  character outside the legend.
    explicit OverworldMap(const MapText& text);

    const std::string& name() const{ return m_name; }
    int width() const{ return m_width; }
    int height() const{ return m_height; }

    bool in_bounds(const MapPoint& point) const;
    //  Out-of-bounds tiles are walls.
    bool wall(const MapPoint& point) const;
    bool water(const MapPoint& point) const;
    bool tall_grass(const MapPoint& point) const;
    bool ledge(const MapPoint& point) const;
    uint8_t elevation(const MapPoint& point) const;
    //  Whether the tile itself refuses a step in `direction`: entering it, or
    //  leaving it.
    bool blocks_entering(const MapPoint& point, MapDirection direction) const;
    bool blocks_leaving(const MapPoint& point, MapDirection direction) const;

    //  The warp the game takes on stepping onto this tile ('C', 'L').
    bool warps_on_step(const MapPoint& point) const;
    //  A door ('D'), entered by walking north into it.
    bool door(const MapPoint& point) const;
    //  The direction that takes the exit mat on this tile, if it is one.
    std::optional<MapDirection> exit_mat(const MapPoint& point) const;
    //  Arriving on a door or cave entrance, the game walks the player one
    //  tile south, out of it.
    bool walks_out_on_arrival(const MapPoint& point) const;

    const MapObject* object_at(const MapPoint& point) const;
    const MapWarp* warp_at(const MapPoint& point) const;
    const std::vector<MapWarp>& warps() const{ return m_warps; }
    const std::vector<MapConnection>& connections() const{ return m_connections; }

    //  Removes the objects the game hides once one of `flags` is set.
    void hide_objects(const std::set<std::string>& flags);

private:
    char terrain(const MapPoint& point) const;

    std::string m_name;
    int m_width = 0;
    int m_height = 0;
    std::vector<char> m_terrain;
    std::vector<uint8_t> m_elevation;
    std::vector<MapWarp> m_warps;
    std::vector<MapObject> m_objects;
    std::vector<MapConnection> m_connections;
};


//  What decides where the player's next step can go.
struct PlayerState{
    MapPoint position;
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
StepKind classify_step(const OverworldMap& map, const PlayerState& state, MapDirection direction);

//  The same for a step that crosses into another map: from `state` on `from`
//  onto `destination` on `to`.
StepKind classify_step_onto(
    const OverworldMap& from, const PlayerState& state, MapDirection direction,
    const OverworldMap& to, const MapPoint& destination
);

//  The player after a step of kind WALK, STOP_SURFING or START_SURF (the
//  Surf action moves the player onto the water).
PlayerState after_step(const OverworldMap& map, const PlayerState& state, MapDirection direction, StepKind kind);
PlayerState after_step_onto(
    const OverworldMap& from, const PlayerState& state,
    const OverworldMap& to, const MapPoint& destination, StepKind kind
);



}
}
}
#endif
