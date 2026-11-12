/*  Connected Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The one world the tile games (BDSP, FRLG) plan and walk on: maps joined by
 *  their warps and their edges (MapData::connections). The player's place in
 *  it is a map and a tile of that map, in the map's own coordinates. Where a
 *  warp or an edge leads is the same in both games; whether a step is
 *  allowed, and what the game does on arrival (FRLG walks the player out of
 *  a door), is each game's.
 *
 */

#ifndef PokemonAutomation_Pokemon_ConnectedMaps_H
#define PokemonAutomation_Pokemon_ConnectedMaps_H

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>
#include "Pokemon_Grid.h"
#include "Pokemon_MapData.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A tile of one of the connected maps: the map's index and the tile in that
//  map's own coordinates.
struct MapPlace{
    size_t map = 0;
    TilePos tile;

    bool operator==(const MapPlace& other) const = default;
};


//  A view of loaded maps as one connected world. It does not own the maps,
//  which must outlive it and keep their order: MapPlace::map indexes them.
class ConnectedMaps{
public:
    explicit ConnectedMaps(const std::vector<MapData>& maps)
        : m_maps(&maps)
    {}

    const MapData& map(size_t index) const{
        return (*m_maps)[index];
    }
    //  The loaded map named `name`, or nothing.
    std::optional<size_t> find(std::string_view name) const;

    //  Where taking `warp` puts the player: its destination point. Nothing
    //  if it leads to a map that isn't loaded or names a point that map
    //  doesn't have.
    std::optional<MapPlace> warp_arrival(const MapWarp& warp) const;

    //  The tile past `side` of `from`'s map that carries on `from`'s row or
    //  column: on the first loaded map connected on that side that has it.
    //  Nothing if no loaded map there has it.
    std::optional<MapPlace> across_edge(const MapPlace& from, Direction side) const;

    //  The tile next to `from` in `direction`: on the same map, or across
    //  its edge. Nothing past an edge that leads nowhere loaded.
    std::optional<MapPlace> neighbour(const MapPlace& from, Direction direction) const;

private:
    const std::vector<MapData>* m_maps;
};


//  Joins the maps that lie on the same world grid (MapData::world) where
//  they meet: each map gets a connection on each side to every map whose
//  rectangle touches that side, unless it has one to that map already.
//  Maps without a world are left alone.
void connect_by_world(std::vector<MapData>& maps);



} // namespace Pokemon
} // namespace PokemonAutomation


//  A hash of MapPlace, for planners that keep places in unordered sets.
template <>
struct std::hash<PokemonAutomation::Pokemon::MapPlace>{
    size_t operator()(const PokemonAutomation::Pokemon::MapPlace& place) const{
        return std::hash<PokemonAutomation::Pokemon::TilePos>()(place.tile) * 31 + place.map;
    }
};

#endif
