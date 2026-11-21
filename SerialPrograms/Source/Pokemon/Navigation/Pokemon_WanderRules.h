/*  Wander Rules
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared helpers for checking whether tiles or movement legs cross a
 *  wandering NPC's `TileBox` (`MapObject::wander`).
 *
 */

#ifndef PokemonAutomation_Pokemon_WanderRules_H
#define PokemonAutomation_Pokemon_WanderRules_H

#include <vector>
#include "Pokemon_Grid.h"
#include "Pokemon_MapData.h"
#include "Pokemon_MapFeatures.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Whether `tile` lies inside `box` (inclusive on all four sides).
inline bool tile_in_box(const TileBox& box, const TilePos& tile){
    return box.x0 <= tile.x && tile.x <= box.x1
        && box.y0 <= tile.y && tile.y <= box.y1;
}

//  The first wandering object on `map` whose wander box contains `point`, or
//  nullptr if none.
inline const MapObject* wander_object_at(const MapData& map, const TilePos& point){
    for (const MapObject& object : map.objects){
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
    const MapData& map, const TilePos& start, Direction direction, int tiles
){
    TilePos tile = start;
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
inline bool leg_touches_wander_box(const std::vector<TilePos>& tiles, const MapData& map){
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
        const MapData* map = map_for(tiles[c]);
        if (map != nullptr && map->tiles.contains(tiles[c].point) && wander_object_at(*map, tiles[c].point) != nullptr){
            return true;
        }
    }
    return false;
}

} // namespace Pokemon
} // namespace PokemonAutomation
#endif
