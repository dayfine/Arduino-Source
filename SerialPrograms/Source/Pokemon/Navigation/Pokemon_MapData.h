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
 *          "allow_running": false,                     //  left out: true
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
 *          "patches": [{"x": 2, "y": 1, "glyph": "#", "measured_by": "run 3, 2026-10-30"}, ...],
 *          "gates": {"door_1": {"tiles": [[7, 9], [7, 10]], "open_glyph": ".", "new_game": "closed",
 *                               "opens_by": "card_key_face_a", "flag": "FLAG_SILPH_5F_DOOR_1"},
 *                    "barrier_1": {"tiles": [[22, 10]], "open_glyph": "~", "new_game": "closed",
 *                                  "switch": "mansion", "open_when": "on"}, ...},
 *          "switches": {"mansion": {"flag": "FLAG_POKEMON_MANSION_SWITCH_STATE", "toggle": true,
 *                                   "floors": ["PokemonMansion_1F", ...],
 *                                   "statues": [{"coord": [5, 5], "face_from": [5, 6], "facing": "north"}]}}
 *      }
 *
 *  A warp may carry "one_way": true (a drop). A map's tiles show each gate
 *  tile as 'G', and every 'G' tile is in a gate.
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
#include "Pokemon_MapFeatures.h"

namespace PokemonAutomation{
namespace Pokemon{


// Complete map dataset including dimensions, collision tiles, warps, and objects.
struct MapData{
    std::string name;
    MapSource source;
    std::optional<MapWorld> world;
    //  Whether the game lets the player run here. FRLG's indoor maps, such as
    //  the Pokemon Centers, don't (pokefirered's map header allow_running):
    //  B held there still walks.
    bool allow_running = true;
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
    //  The barriers the story opens, by id. Every GATE tile of `tiles` is in
    //  exactly one gate.
    std::map<std::string, MapGate> gates;
    //  The switches (by id) that gates of this map follow or that have a
    //  statue on it.
    std::map<std::string, MapSwitch> switches;
};

//  Reads legacy/unmarked v1 or compact pokesim-runtime-map/2 JSON.
//  Both games use this entry point; `source_name` names it in errors.
//  Throws ParseException, naming the source and the JSON path (e.g.
//  "Maps/TestTown.json: warps[2].x is missing."), when a field is missing,
//  doesn't fit the schema, or places something outside the tiles.
MapData parse_map(const JsonValue& json, const std::string& source_name);

//  The warp on `pos`, or nullptr if there is none.
const MapWarp* warp_at(const MapData& map, const TilePos& pos);

//  Counts the number of tiles along `direction` starting from `from` until reaching
//  a DOOR tile, an impassable tile (WALL, OUTSIDE), or the edge of the grid.
//  Used by AutoStory walkers to bound holds into door warps based on distance.
size_t tiles_to_door(const Grid<TileKind>& tiles, const TilePos& from, Direction direction, size_t max_tiles = 50);

//  Convenience overload that queries map.tiles.
size_t tiles_to_door(const MapData& map, const TilePos& from, Direction direction, size_t max_tiles = 50);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
