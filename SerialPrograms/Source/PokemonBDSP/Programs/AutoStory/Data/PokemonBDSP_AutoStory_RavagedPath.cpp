/*  BDSP AutoStory - Ravaged Path tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_MapGrids.h"
#include "PokemonBDSP_AutoStory_RavagedPath.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace RavagedPath{

using namespace Pokemon;


//  Map source: rows 30 to 53 from the decomp (map_data_435/436, 2026-10-31);
//      the rows above from the DP image, unused by any route.
//  Console checks: entrance (19,50), the stairs, the three-wide corridor, the
//      rocks, the east end and the exit tile (2026-10-31 probes).
//  Status: 01.09 reaches the exit by hand; no program pass yet.
//
//  Rows 30 to 53 are decoded from Pokemon Platinum's collision table for
//  this map (the pret/pokeplatinum decompilation, land data 435/436 of map
//  matrix 051) with the objects from its events file; BDSP keeps the layout.
//  Water and the items on the ground are '#'; the smashable rocks are '#'
//  too and listed in rocks(). Checked on the console 2026-10-31 (r235b and
//  D-pad probes): in from the south door onto (19, 50), up the stairs
//  (rows 45 to 47, columns 18 to 20) and north to the wall at row 39; the
//  corridor is three wide up to row 41 and opens east on rows 42 to 44,
//  with the rocks at (22, 42) and (22, 43), the Potion ball at (22, 44) and
//  a rock at (23, 44). Rows 0 to 29 are still the hand trace from
//  Bulbapedia's Ravaged_Path_DP.png (stretched from 528x672 to 528x864), with
//  '?' where the untraced west chambers continue; nothing we walk is there.
const TileGrid& grid(){
    return story_grid("RavagedPath");
}

const std::vector<TilePos>& rocks(){
    static const std::vector<TilePos> tiles{
        {11, 36}, {7, 37}, {12, 37}, {8, 38}, {10, 38}, {10, 39}, {13, 39},
        {7, 40}, {9, 40}, {17, 40}, {12, 41}, {22, 42}, {22, 43}, {23, 44},
    };
    return tiles;
}

const TilePos ENTRANCE{19, 50};

const std::vector<TilePos>& before_rocks(){
    static const std::vector<TilePos> tiles{{21, 42}, {21, 43}};
    return tiles;
}

const TilePos EAST_END{29, 44};
const TilePos ABOVE_EXIT{28, 44};


}
}
}
}
