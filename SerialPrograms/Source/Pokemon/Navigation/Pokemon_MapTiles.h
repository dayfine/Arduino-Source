/*  Map Tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A map's tile grid. Pokemon_MapData.h builds on it.
 *
 */

#ifndef PokemonAutomation_Pokemon_MapTiles_H
#define PokemonAutomation_Pokemon_MapTiles_H

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
// Returns the character glyph representing a TileKind.
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



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
