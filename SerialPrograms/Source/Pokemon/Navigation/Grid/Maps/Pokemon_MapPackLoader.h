/*  Map Pack Loader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared helpers for loading embedded JSON map packs into Pokemon::MapData,
 *  querying named points/regions/flag sets, and cropping sub-maps.
 *
 */

#ifndef PokemonAutomation_Pokemon_MapPackLoader_H
#define PokemonAutomation_Pokemon_MapPackLoader_H

#include <functional>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon_MapData.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Finds `name` in `embedded_json` (pairs of map name -> JSON text), parses it
//  with Pokemon::parse_map(), and checks that the parsed map's name matches
//  `name`. `source_prefix` is prepended to "<name>.json" in error messages.
MapData load_embedded_map(
    std::span<const std::pair<std::string, std::string>> embedded_json,
    std::string_view name,
    std::string_view source_prefix = {}
);

//  Parses every entry of `embedded_json` into a name-keyed map, verifying each
//  parsed map's `name` field matches its key.
std::map<std::string, MapData, std::less<>> load_embedded_map_pack(
    std::span<const std::pair<std::string, std::string>> embedded_json,
    std::string_view source_prefix = {}
);

//  Looks up named `point` on `map`; throws InternalProgramError if missing.
TilePos require_map_point(const MapData& map, std::string_view point);

//  Looks up named `point` on the map named `map_name` in `maps`; throws
//  InternalProgramError if the map or point is missing.
TilePos require_map_point(
    std::span<const MapData> maps,
    std::string_view map_name,
    std::string_view point
);

//  Collects numbered points "<region>.1", "<region>.2", ... from `map.points`;
//  throws InternalProgramError if none exist.
std::vector<TilePos> require_numbered_map_region(
    const MapData& map,
    std::string_view region
);

//  Unions the flag_sets entry named `flag_set_name` across all `maps`.
std::set<std::string> collect_map_flag_set(
    std::span<const MapData> maps,
    std::string_view flag_set_name
);

//  Crops a `width` x `height` sub-map from `map` starting at `corner`,
//  shifting tiles, optional elevation, objects, and warps into cropped coords.
MapData crop_map_data(
    const MapData& map,
    const TilePos& corner,
    int width,
    int height
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
