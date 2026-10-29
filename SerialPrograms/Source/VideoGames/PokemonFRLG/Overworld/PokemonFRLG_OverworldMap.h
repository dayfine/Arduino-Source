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


//  Stepping onto (x, y) takes the player to warp `dest_warp` of `dest_map`.
//  An empty `dest_map` leads to a map that isn't loaded (e.g. the city
//  outside a cave).
struct MapWarp{
    int x = 0;
    int y = 0;
    std::string dest_map;
    int dest_warp = 0;
};

//  An object standing on a tile (item ball, rock, Pokemon), which blocks it.
struct MapObject{
    int x = 0;
    int y = 0;
    std::string name;
};

//  A map as generated from the decompilation.
struct MapText{
    std::string name;
    //  One character per tile:
    //    '#'  collision (wall, cliff)
    //    '.'  floor (including ladders and stairs)
    //    '~'  water: entered only by starting to Surf
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
    uint8_t elevation(const MapPoint& point) const;
    //  Whether the tile itself refuses a step in `direction`: entering it, or
    //  leaving it.
    bool blocks_entering(const MapPoint& point, MapDirection direction) const;
    bool blocks_leaving(const MapPoint& point, MapDirection direction) const;

    const MapObject* object_at(const MapPoint& point) const;
    const MapWarp* warp_at(const MapPoint& point) const;
    const std::vector<MapWarp>& warps() const{ return m_warps; }

private:
    char terrain(const MapPoint& point) const;

    std::string m_name;
    int m_width = 0;
    int m_height = 0;
    std::vector<char> m_terrain;
    std::vector<uint8_t> m_elevation;
    std::vector<MapWarp> m_warps;
    std::vector<MapObject> m_objects;
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

//  The player after a step of kind WALK, STOP_SURFING or START_SURF (the
//  Surf action moves the player onto the water).
PlayerState after_step(const OverworldMap& map, const PlayerState& state, MapDirection direction, StepKind kind);



}
}
}
#endif
