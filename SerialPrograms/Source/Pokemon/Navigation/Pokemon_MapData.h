/*  Map Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A map as a navigation program sees it, read from the one JSON schema the
 *  tile games (BDSP, FRLG) share: the tiles as rows of glyphs, the named
 *  points and regions routes start and end at, the warps, the objects
 *  standing on the map, the scenes that start on stepping somewhere, the
 *  connections to neighbouring maps, and patches that put tiles measured on
 *  the console on top of the decompilation's data.
 *
 *      {
 *          "name": "TestTown",
 *          "source": {"decomp_file": "data/maps/TestTown/map.json", "revision": "abc123", "date": "2026-10-31"},
 *          "world": {"name": "Sinnoh", "x": 96, "y": 864},
 *          "tiles": ["####", "#..~", ...],             //  rows of glyphs, all the same length
 *          "elevation": ["3333", "3300", ...],         //  one hex digit per tile
 *          "points": {"door": [1, 2], ...},
 *          "regions": {"lawn": [[1, 1], [2, 1]], ...},
 *          "warps": [{"x": 1, "y": 2, "enter": "walk_into", "to_map": "TestHouse", "to_point": "exit"}, ...],
 *          "objects": [{"name": "mom", "kind": "npc", "x": 2, "y": 1,
 *                       "facing": "south", "wander": {"x0": 1, "y0": 1, "x1": 2, "y1": 2},
 *                       "trainer": {"sight": 4, "looks": ["south", "west"]},
 *                       "hide_flag": "FLAG_HIDE_MOM"}, ...],
 *          "triggers": [{"name": "mom_warning", "box": {"x0": 1, "y0": 2, "x1": 1, "y1": 2}, "when": "00.01"}, ...],
 *          "flag_sets": {"after_00.01": ["FLAG_HIDE_MOM", ...], ...},
 *          "connections": [{"side": "east", "to_map": "Route 1", "offset": 0}, ...],
 *          "patches": [{"x": 2, "y": 1, "glyph": "#", "measured_by": "run 3, 2026-10-30"}, ...]
 *      }
 *
 *  `name`, `source` and `tiles` are required; everything else may be left
 *  out. Which game's planner reads these maps, and how, is the game's
 *  business.
 *
 */

#ifndef PokemonAutomation_Pokemon_MapData_H
#define PokemonAutomation_Pokemon_MapData_H

#include <cstdint>
#include <map>
#include <optional>
#include <set>
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
    DOOR,           //  'D'  a door or warp tile walked into
    SIGN,           //  'S'  a sign, read by facing it
    OUTSIDE,        //  ' '  no map of the game's here (a map's rectangle
                    //       reaching past the game's own)
    CLOSED_NORTH,   //  '8'  open, but not crossed through its named side:
    CLOSED_SOUTH,   //  '2'  not entered from it, nor left through it (the
    CLOSED_WEST,    //  '4'  number pad's arrows)
    CLOSED_EAST,    //  '6'
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

//  Where the map lies on a world grid that several maps share (a BDSP map
//  matrix): its tile (0, 0) is tile `offset` of world `name`. Maps on the
//  same world join where they meet.
struct MapWorld{
    std::string name;
    TilePos offset;
};

//  Where a warp puts the player: point `point` of map `map`.
struct WarpDestination{
    std::string map;
    std::string point;
};

//  How the player takes a warp, as the map file writes it.
enum class WarpEntry{
    WALK_INTO,      //  "walk_into": a door or cave mouth, walked into
    STEP_ON,        //  "step_on": stepping onto the tile (ladders,
                    //  escalators, BDSP's warp tiles)
    CAVE_ENTRANCE,  //  "cave_entrance": stepping onto the tile; the game
                    //  walks a player arriving on it a tile south
    WALK_NORTH,     //  "walk_north" ...: standing on the tile and walking
    WALK_SOUTH,     //  that way (exit mats, stairs at the side of a room)
    WALK_WEST,
    WALK_EAST,
};

//  A warp from (x, y), taken as `enter` says. `to` is left out for a warp
//  to a map that is not loaded: the warp is still there, for routes to keep
//  off.
struct MapWarp{
    int x = 0;
    int y = 0;
    WarpEntry enter = WarpEntry::WALK_INTO;
    std::optional<WarpDestination> to;
};

enum class ObjectKind{
    NPC,
    ITEM,
    SIGN,
};

//  A box of tiles, both corners inclusive.
struct TileBox{
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

//  A trainer sees `sight` tiles ahead in each direction of `looks` (their
//  facing, unless the map says otherwise).
struct MapTrainer{
    int sight = 0;
    std::vector<Direction> looks;
};

//  Something standing on a tile, which blocks it. A person who wanders may
//  stand anywhere in `wander`. The game hides the object while `hide_flag`
//  is set (an event flag, or a story state the game's code sets).
struct MapObject{
    std::string name;
    ObjectKind kind = ObjectKind::NPC;
    int x = 0;
    int y = 0;
    std::optional<Direction> facing;
    std::optional<TileBox> wander;
    std::optional<MapTrainer> trainer;
    std::optional<std::string> hide_flag;
};

//  A scene that starts when the player steps into `box` while the story is
//  in `when` (any time, if left out): the decompilation's coord events.
struct MapTrigger{
    std::string name;
    TileBox box;
    std::optional<std::string> when;
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
    std::optional<MapWorld> world;
    //  With the patches applied.
    Grid<TileKind> tiles;
    //  The same size as `tiles`. FRLG reads a map without it as all ground
    //  (elevation 3); what the values mean is the game's business.
    std::optional<Grid<uint8_t>> elevation;
    std::map<std::string, TilePos> points;
    //  Named sets of tiles, in the order the map lists them.
    std::map<std::string, std::vector<TilePos>> regions;
    std::vector<MapWarp> warps;
    std::vector<MapObject> objects;
    std::vector<MapTrigger> triggers;
    //  Flags set together, by name (e.g. "after_hall_of_fame").
    std::map<std::string, std::set<std::string>> flag_sets;
    std::vector<MapConnection> connections;
    std::vector<MapPatch> patches;
};

//  Reads a map from its parsed JSON. `source_name` names it in errors.
//  Throws ParseException, naming the source and the JSON path (e.g.
//  "Maps/TestTown.json: warps[2].x is missing."), when a field is missing,
//  doesn't fit the schema, or places something outside the tiles.
MapData parse_map(const JsonValue& json, const std::string& source_name);

//  The warp on `pos`, or nullptr if there is none.
const MapWarp* warp_at(const MapData& map, const TilePos& pos);



}
}
#endif
