/*  Overworld Wild Tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The tiles where a step may start a wild battle: tall grass, and the
 *  tiles a map lists in its WILD_REGION.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWildTiles_H
#define PokemonAutomation_PokemonFRLG_OverworldWildTiles_H

#include <string_view>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "PokemonFRLG_OverworldTiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The map region of the tiles with wild encounters whose glyph isn't tall
//  grass.
inline constexpr std::string_view WILD_REGION = "wild";
//  Whether a step onto the tile may start a wild battle: tall grass, or a
//  tile in WILD_REGION.
bool is_tall_grass(const MapData& map, const TilePos& point);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
