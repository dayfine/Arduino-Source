/*  Connected Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Pokemon_ConnectedMaps.h"

namespace PokemonAutomation{
namespace Pokemon{


std::optional<size_t> ConnectedMaps::find(std::string_view name) const{
    for (size_t index = 0; index < m_maps->size(); index++){
        if ((*m_maps)[index].name == name){
            return index;
        }
    }
    return std::nullopt;
}

std::optional<MapPlace> ConnectedMaps::warp_arrival(const MapWarp& warp) const{
    if (!warp.to){
        return std::nullopt;
    }
    const std::optional<size_t> destination = find(warp.to->map);
    if (!destination){
        return std::nullopt;
    }
    const MapData& to = map(*destination);
    const auto arrival = to.points.find(warp.to->point);
    if (arrival == to.points.end()){
        return std::nullopt;
    }
    return MapPlace{*destination, arrival->second};
}

std::optional<MapPlace> ConnectedMaps::across_edge(const MapPlace& from, Direction side) const{
    const TilePos here = from.tile;
    for (const MapConnection& connection : map(from.map).connections){
        if (connection.side != side){
            continue;
        }
        const std::optional<size_t> destination = find(connection.to_map);
        if (!destination){
            continue;
        }
        const Grid<TileKind>& to = map(*destination).tiles;
        TilePos point{here.x - connection.offset, here.y - connection.offset};
        switch (side){
        case Direction::NORTH:
            point.y = to.height - 1;
            break;
        case Direction::SOUTH:
            point.y = 0;
            break;
        case Direction::WEST:
            point.x = to.width - 1;
            break;
        case Direction::EAST:
            point.x = 0;
            break;
        }
        if (to.contains(point)){
            return MapPlace{*destination, point};
        }
    }
    return std::nullopt;
}

std::optional<MapPlace> ConnectedMaps::neighbour(const MapPlace& from, Direction direction) const{
    const TilePos ahead = from.tile.next(direction);
    if (map(from.map).tiles.contains(ahead)){
        return MapPlace{from.map, ahead};
    }
    return across_edge(from, direction);
}


namespace{

//  The side of `a` that `b` touches, and the offset of `b`'s tiles along it,
//  if their rectangles on the world grid meet along a side.
std::optional<MapConnection> touching(const MapData& a, const MapData& b){
    const TilePos pa = a.world->offset;
    const TilePos pb = b.world->offset;
    const int aw = a.tiles.width;
    const int ah = a.tiles.height;
    const int bw = b.tiles.width;
    const int bh = b.tiles.height;
    const bool columns_overlap = pa.x < pb.x + bw && pb.x < pa.x + aw;
    const bool rows_overlap = pa.y < pb.y + bh && pb.y < pa.y + ah;
    if (columns_overlap && pb.y + bh == pa.y){
        return MapConnection{Direction::NORTH, b.name, pb.x - pa.x};
    }
    if (columns_overlap && pb.y == pa.y + ah){
        return MapConnection{Direction::SOUTH, b.name, pb.x - pa.x};
    }
    if (rows_overlap && pb.x + bw == pa.x){
        return MapConnection{Direction::WEST, b.name, pb.y - pa.y};
    }
    if (rows_overlap && pb.x == pa.x + aw){
        return MapConnection{Direction::EAST, b.name, pb.y - pa.y};
    }
    return std::nullopt;
}

} // namespace

void connect_by_world(std::vector<MapData>& maps){
    for (MapData& a : maps){
        for (const MapData& b : maps){
            if (&a == &b || !a.world || !b.world || a.world->name != b.world->name){
                continue;
            }
            const std::optional<MapConnection> connection = touching(a, b);
            if (!connection){
                continue;
            }
            const bool joined = std::ranges::any_of(a.connections, [&](const MapConnection& existing){
                return existing.to_map == b.name;
            });
            if (!joined){
                a.connections.push_back(*connection);
            }
        }
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
