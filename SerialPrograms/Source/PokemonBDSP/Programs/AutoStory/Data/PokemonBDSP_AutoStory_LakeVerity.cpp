/*  BDSP AutoStory - Lake Verity tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_LakeVerity.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace LakeVerity{

using namespace Pokemon;


//  Map source: the decomp (Diamond land_data 0512 = Platinum map_data_541,
//      2026-10-31) with the objects from its events file.
//  Console checks: none yet.
//  Status: no checkpoint passes on this grid yet.
//
//  Decoded from Pokemon Diamond's collision table for this map (the
//  pret/pokediamond decompilation,
//  files/fielddata/land_data/land_data_release/narc_0512: the cell at column
//  1, row 1 of the lake's own matrix 101, which is 3 x 2 cells; map_data_541
//  of matrix 102 in pret/pokeplatinum), with the objects from its events file
//  (zone_event narc_0298, events_lake_verity.json in Platinum); BDSP keeps
//  Diamond/Pearl's layout. Local (x, y) = world (x - 32, z - 32) in that
//  matrix; 32 x 32 tiles. Only this cell is reachable on foot: the shore
//  where the starter scene plays, columns 10 to 25, rows 4 to 22. The lake is
//  '#'; the tiles at (0..6, 4..9) are shallow water (puddle, behaviour 0x16),
//  walkable in the data but closed in by the lake. The west shore with TM38
//  (cells to the west) needs Surf.
//  Entrance: the warp tiles (14, 22) and (15, 22), arrived on from Verity
//  Lakefront, written as '.'; walking south off them leaves. The scene's
//  actors are hidden-flag objects, drawn where the events file puts them
//  because that is where the walk ends: Rowan and the briefcase on (16, 11)
//  and the player's counterpart on (17, 11), at the water's edge. Objects,
//  from the events file:
//    Prof. Rowan (16, 11), looks north, hidden by flag 403; the briefcase on
//    the same tile, hidden by flag 404;
//    the counterpart (Dawn/Lucas, object "Var 0") (17, 11), looks north,
//    hidden by flag 434;
//    Starly (16, 19), looks north, hidden by flag 429, drawn as 'N' (a scene
//    actor on open ground);
//    Starly (25, 11), looks west, hidden by flag 429, perched on a blocked
//    tile: stays '#';
//    Barry (15, 22), looks north, hidden by flag 436, on the entrance warp
//    tile: '.' wins;
//    no one wanders here, and no trainers.
//  Platinum differs: it stages its scene elsewhere (Rowan (21, 7), the
//  counterpart (22, 6), Mars (23, 6), four Galactic grunts with sight 4 to 6,
//  no Starly or Barry), walls off the puddle tiles at (0..6, 4..9) and opens
//  a path to the Verity Cavern warp at (0, 0). The shore, columns 10 to 25,
//  decodes the same tile for tile. Diamond keeps its Galactic version as a
//  separate map (MAP_LAKE_VERITY_GALACTIC), reached from the same Lakefront
//  warp tiles.
const TileGrid& grid(){
    static const TileGrid grid({
    //     0         1         2         3
        R"(################################)",  //   0
        R"(################################)",  //   1
        R"(################################)",  //   2
        R"(################################)",  //   3
        R"(......############....##########)",  //   4
        R"(......############....##########)",  //   5
        R"(.......###########......########)",  //   6
        R"(.......###########......########)",  //   7
        R"(.......###########".....########)",  //   8
        R"(.......###########""....########)",  //   9
        R"(################""""....########)",  //  10
        R"(################NN""....########)",  //  11
        R"(##############."""""..##########)",  //  12
        R"(##############.""""...##########)",  //  13
        R"(############..."""..############)",  //  14
        R"(############........############)",  //  15
        R"(##########..........############)",  //  16
        R"(##########..........############)",  //  17
        R"(############......##############)",  //  18
        R"(############....N.##############)",  //  19
        R"(############......##############)",  //  20
        R"(############......##############)",  //  21
        R"(##############..################)",  //  22
        R"(################################)",  //  23
        R"(################################)",  //  24
        R"(################################)",  //  25
        R"(################################)",  //  26
        R"(################################)",  //  27
        R"(################################)",  //  28
        R"(################################)",  //  29
        R"(################################)",  //  30
        R"(################################)",  //  31
    });
    return grid;
}

}
}
}
}
