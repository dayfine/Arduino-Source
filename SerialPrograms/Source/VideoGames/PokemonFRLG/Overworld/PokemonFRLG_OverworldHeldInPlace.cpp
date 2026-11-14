/*  Overworld Held In Place
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include "PokemonFRLG_OverworldHeldInPlace.h"
#include "PokemonFRLG_OverworldStepChecks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const MapData* map_named(const std::vector<MapData>& maps, const std::string& name){
    const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapData& data){
        return data.name == name;
    });
    return map == maps.end() ? nullptr : &*map;
}

//  `tile` moved `offset` tiles square to `direction`: one way for a
//  positive offset, the other for a negative one.
TilePos beside(TilePos tile, Direction direction, int offset){
    const Direction side = (Direction)(((int)direction + (offset < 0 ? 3 : 1)) % 4);
    for (int c = 0; c < std::abs(offset); c++){
        tile = tile.next(side);
    }
    return tile;
}

} // namespace


std::vector<StopCandidate> before_signs(const std::vector<MapData>& maps, const std::vector<MapPosition>& starts, const RouteStep& step){
    std::vector<StopCandidate> candidates;
    if (step.direction != Direction::NORTH && step.direction != Direction::SOUTH){
        return candidates;
    }
    RouteStep held = step;
    held.tiles += OPEN_LEG_SHORTFALL;
    for (const MapPosition& start : starts){
        const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapData& data){
            return data.name == start.map;
        });
        if (map == maps.end() || is_wall(*map, start.point)){
            continue;
        }
        const std::vector<MapPosition> tiles = leg_tiles(maps, start, held);
        if (tiles.empty() || tiles.back().map != start.map
            || tile_at(*map, tiles.back().point.next(step.direction)) != TileKind::SIGN
        ){
            continue;
        }
        const StopCandidate candidate{tiles.back(), std::nullopt};
        if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()){
            candidates.push_back(candidate);
        }
    }
    return candidates;
}

std::vector<StopCandidate> before_people(
    const std::vector<MapData>& maps, const std::vector<MapPosition>& tiles, const RouteStep& hold, int across
){
    std::vector<StopCandidate> candidates;
    for (const MapPosition& tile : tiles){
        const MapData* map = map_named(maps, tile.map);
        if (map == nullptr){
            continue;
        }
        for (int offset = -across; offset <= across; offset++){
            const TilePos at = beside(tile.point, hold.direction, offset);
            const TilePos ahead = at.next(hold.direction);
            const bool stands = in_bounds(*map, at) && !is_wall(*map, at) && object_at(*map, at) == nullptr
                && is_water(*map, at) == hold.surfing;
            if (!stands || !in_bounds(*map, ahead) || object_at(*map, ahead) == nullptr){
                continue;
            }
            const StopCandidate candidate{MapPosition{tile.map, at}, std::nullopt};
            if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()){
                candidates.push_back(candidate);
            }
        }
    }
    return candidates;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
