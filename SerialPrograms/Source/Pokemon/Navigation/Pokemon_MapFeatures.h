/*  Map Features
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The features a map's tiles can carry. Pokemon_MapData.h builds on it.
 *
 */

#ifndef PokemonAutomation_Pokemon_MapFeatures_H
#define PokemonAutomation_Pokemon_MapFeatures_H

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_Grid.h"
#include "Pokemon_MapTiles.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A warp from (x, y), taken as `enter` says. `to` is left out for a warp
//  to a map that is not loaded: the warp is still there, for routes to keep
//  off.
struct MapWarp{
    int x = 0;
    int y = 0;
    WarpEntry enter = WarpEntry::WALK_INTO;
    std::optional<WarpDestination> to;
    //  A drop (a hole the player falls through): the landing has no way back
    //  up, so a route takes it forward only.
    bool one_way = false;
};

// Type classification for interactive map objects.
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



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
