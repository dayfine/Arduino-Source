/*  Fly Map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The towns Fly takes the player to in Kanto: each one's cell on the fly
 *  map and the tile the player lands on. Cells are pokefirered's
 *  sRegionMapSections_Kanto (src/data/region_map/region_map_layout_kanto.h,
 *  22 x 15, the cursor's x and y); landing tiles its heal locations
 *  (src/data/heal_locations.json), which Fly warps to (SetFlyWarpDestination()
 *  in src/region_map.c).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_FlyMap_H
#define PokemonAutomation_PokemonFRLG_FlyMap_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_OverworldRouteSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A place on the fly map Fly can take the player to.
struct FlyTown{
    //  The map's name, as a step names the town ("CeladonCity", "Route4").
    std::string name;
    //  The town's cell on the fly map: the presses right and down from its
    //  top-left corner, where a hold up-left leaves the cursor
    //  (HandleRegionMapInput() stops it at 0).
    TilePos cell;
    //  The tile Fly lands the player on: below the Pokemon Center's door,
    //  or Pallet Town's player's house.
    MapPosition landing;
};

//  Kanto's Fly destinations, Pallet Town first.
const std::vector<FlyTown>& kanto_fly_towns();
//  The index of the town named `name` in kanto_fly_towns(), if it is one.
std::optional<size_t> fly_town_index(std::string_view name);

//  The fly map's width in cells (MAP_WIDTH in region_map.c).
constexpr int FLY_MAP_COLUMNS = 22;
//  The fly map's height in cells (MAP_HEIGHT in region_map.c).
constexpr int FLY_MAP_ROWS = 15;



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
