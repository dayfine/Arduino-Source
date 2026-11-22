/*  Wander Rules
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Helpers for checking whether tiles or movement legs cross a wandering
 *  NPC's `TileBox` (`MapObject::wander`).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_WanderRules_H
#define PokemonAutomation_PokemonFRLG_WanderRules_H

#include <vector>
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Grid.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_MapData.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_MapFeatures.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether `tile` lies inside `box` (inclusive on all four sides).
inline bool tile_in_box(const Pokemon::TileBox& box, const Pokemon::TilePos& tile){
    return box.x0 <= tile.x && tile.x <= box.x1
        && box.y0 <= tile.y && tile.y <= box.y1;
}

//  The first wandering object on `map` whose wander box contains `point`, or
//  nullptr if none.
inline const Pokemon::MapObject* wander_object_at(const Pokemon::MapData& map, const Pokemon::TilePos& point){
    for (const Pokemon::MapObject& object : map.objects){
        if (object.wander && tile_in_box(*object.wander, point)){
            return &object;
        }
    }
    return nullptr;
}

//  Whether any stepped tile of a straight leg from `start` walking `tiles`
//  steps in `direction` (excluding `start` itself) lies inside a wander box on
//  `map`.
inline bool leg_touches_wander_box(
    const Pokemon::MapData& map, const Pokemon::TilePos& start, Pokemon::Direction direction, int tiles
){
    Pokemon::TilePos tile = start;
    for (int c = 0; c < tiles; c++){
        tile = tile.next(direction);
        if (map.tiles.contains(tile) && wander_object_at(map, tile) != nullptr){
            return true;
        }
    }
    return false;
}

//  Whether any stepped tile in `tiles[1..]` (excluding `tiles[0]`, the start)
//  lies inside a wander box on `map`.
inline bool leg_touches_wander_box(const std::vector<Pokemon::TilePos>& tiles, const Pokemon::MapData& map){
    for (size_t c = 1; c < tiles.size(); c++){
        if (map.tiles.contains(tiles[c]) && wander_object_at(map, tiles[c]) != nullptr){
            return true;
        }
    }
    return false;
}

//  Multi-map overload: `tiles` is a sequence of positions (each with `.point`)
//  starting at `tiles[0]`, and `map_for(position)` returns `const MapData*`.
template <typename Position, typename MapLookup>
bool leg_touches_wander_box(const std::vector<Position>& tiles, MapLookup&& map_for);

template <typename Position, typename MapLookup>
bool leg_touches_wander_box(const std::vector<Position>& tiles, MapLookup&& map_for){
    for (size_t c = 1; c < tiles.size(); c++){
        const Pokemon::MapData* map = map_for(tiles[c]);
        if (map != nullptr && map->tiles.contains(tiles[c].point) && wander_object_at(*map, tiles[c].point) != nullptr){
            return true;
        }
    }
    return false;
}

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
