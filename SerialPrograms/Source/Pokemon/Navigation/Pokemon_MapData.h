/*  Map Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A map as a navigation program sees it, read from the one JSON schema the
 *  tile games (BDSP, FRLG) share: the tiles as rows of glyphs, the named
 *  points routes start and end at, the warps, the objects standing on the
 *  map, the connections to neighbouring maps, and patches that put tiles
 *  measured on the console on top of the decompilation's data.
 *
 *      {
 *          "name": "TestTown",
 *          "source": {"decomp_file": "data/maps/TestTown/map.json", "revision": "abc123", "date": "2026-10-31"},
 *          "tiles": ["####", "#..~", ...],             //  rows of glyphs, all the same length
 *          "points": {"door": [1, 2], ...},
 *          "warps": [{"x": 1, "y": 2, "to_map": "TestHouse", "to_point": "exit"}, ...],
 *          "objects": [{"name": "mom", "kind": "npc", "x": 2, "y": 1,
 *                       "facing": "south", "wander": {"x0": 1, "y0": 1, "x1": 2, "y1": 2}}, ...],
 *          "connections": [{"side": "east", "to_map": "Route 1", "offset": 0}, ...],
 *          "patches": [{"x": 2, "y": 1, "glyph": "#", "measured_by": "run 3, 2026-10-30"}, ...]
 *      }
 *
 *  `name`, `source` and `tiles` are required; the lists may be left out.
 *  Which game's planner reads these maps, and how, is the game's business.
 *
 */

#ifndef PokemonAutomation_Pokemon_MapData_H
#define PokemonAutomation_Pokemon_MapData_H

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a tile is, for walking. The map files write each as one glyph.
enum class TileKind : uint8_t{
    WALL,           //  '#'
    OPEN,           //  '.'
    TALL_GRASS,     //  '~'  open, with wild encounters
    WATER,          //  'w'
    LEDGE_NORTH,    //  '^'  a wall, except that walking into it the arrow's
    LEDGE_SOUTH,    //  'v'  way hops the player over it
    LEDGE_WEST,     //  '<'
    LEDGE_EAST,     //  '>'
    DOOR,           //  'D'  a door or warp tile
    SIGN,           //  'S'  a sign, read by facing it
};
//  The tile a glyph stands for, or nothing if it isn't one.
std::optional<TileKind> tile_from_glyph(char glyph);
char glyph_of(TileKind tile);


//  Where the tiles came from: the decompilation file, at which revision,
//  read when.
struct MapSource{
    std::string decomp_file;
    std::string revision;
    std::string date;
};

//  Stepping onto (x, y) puts the player at point `to_point` of `to_map`.
struct MapWarp{
    int x = 0;
    int y = 0;
    std::string to_map;
    std::string to_point;
};

enum class ObjectKind{
    NPC,
    ITEM,
    SIGN,
};

//  The tiles a wanderer may stand on, both corners inclusive.
struct WanderBox{
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

//  Something standing on a tile, which blocks it. A person who wanders may
//  stand anywhere in `wander`.
struct MapObject{
    std::string name;
    ObjectKind kind = ObjectKind::NPC;
    int x = 0;
    int y = 0;
    std::optional<Direction> facing;
    std::optional<WanderBox> wander;
};

//  Walking off the map's `side` continues on `to_map`, whose tiles are
//  shifted by `offset` along that side.
struct MapConnection{
    Direction side = Direction::NORTH;
    std::string to_map;
    int offset = 0;
};

//  A tile measured on the console that differs from the decompilation's.
struct MapPatch{
    int x = 0;
    int y = 0;
    TileKind glyph = TileKind::WALL;
    std::string measured_by;
};

struct MapData{
    std::string name;
    MapSource source;
    //  With the patches applied.
    Grid<TileKind> tiles;
    std::map<std::string, TilePos> points;
    std::vector<MapWarp> warps;
    std::vector<MapObject> objects;
    std::vector<MapConnection> connections;
    std::vector<MapPatch> patches;
};

//  Reads a map from its parsed JSON. `source_name` names it in errors.
//  Throws ParseException, naming the source and the JSON path (e.g.
//  "Maps/TestTown.json: warps[2].x is missing."), when a field is missing,
//  doesn't fit the schema, or places something outside the tiles.
MapData parse_map(const JsonValue& json, const std::string& source_name);



}
}
#endif
