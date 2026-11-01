/*  BDSP AutoStory - Verity Lakefront tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_VerityLakefront.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace VerityLakefront{

using namespace Pokemon;


//  Map source: the decomp (Diamond land_data 0004 = Platinum map_data_004,
//      2026-10-31) with the objects from its events file.
//  Console checks: none yet.
//  Status: no checkpoint passes on this grid yet.
//
//  Decoded from Pokemon Diamond's collision table for this map (the
//  pret/pokediamond decompilation,
//  files/fielddata/land_data/land_data_release/narc_0004: one cell of the
//  Sinnoh matrix at column 2, row 26; map_data_004 in pret/pokeplatinum),
//  with the objects from its events file (zone_event narc_0319,
//  events_verity_lakefront.json in Platinum); BDSP keeps Diamond/Pearl's
//  layout. Local (x, y) = world (x - 64, z - 832); 32 x 32 tiles. The map
//  header covers four cells (columns 1 and 2, rows 25 and 26); the other
//  three are trees and an 8 x 8 patch of shallow water, so only this one is
//  kept. The pocket (0..3, 0..3) is that water (puddle, behaviour 0x16),
//  closed in.
//  Route 201's west edge is this cell's column 31: open on rows 14 to 18 (the
//  upper level) and 20 to 23 (the lower), with Route 201's x = 96 at our x =
//  32. From the upper level the way west is over the ledge at column 20 (rows
//  14 to 18, jumped west, shown as a wall), landing on columns 14 to 19; the
//  lower level joins it at (16..19, 20). The ledges on row 19, 'v', drop
//  south onto the lower level. The lake's entrance is the warp tiles (16, 11)
//  and (17, 11), walked onto heading north, written as '.'; their destination
//  is Lake Verity, or its Galactic version once that story state is set.
//  Objects, from the events file:
//    arrow signpost (15, 14), beside the path to the lake;
//    Barry (16, 11), looks south, hidden by flag 435, standing on the warp
//    tile: '.' wins;
//    no one else, and no trainers.
//  Platinum decodes the same terrain; it has no Barry object here and calls
//  the sign a trainer-tips signpost.
const TileGrid& grid(){
    static const TileGrid grid({
    //     0         1         2         3
        R"(....############################)",  //   0
        R"(....############################)",  //   1
        R"(....############################)",  //   2
        R"(....############################)",  //   3
        R"(################################)",  //   4
        R"(################################)",  //   5
        R"(################################)",  //   6
        R"(################################)",  //   7
        R"(################################)",  //   8
        R"(################################)",  //   9
        R"(################################)",  //  10
        R"(################..##############)",  //  11
        R"(################..##############)",  //  12
        R"(################..##############)",  //  13
        R"(##############.S....#...........)",  //  14
        R"(##############......#.....####..)",  //  15
        R"(##############......#.....####..)",  //  16
        R"(##############......#...........)",  //  17
        R"(##############......#...........)",  //  18
        R"(##############......#vvvvv##vvvv)",  //  19
        R"(################..........##....)",  //  20
        R"(################................)",  //  21
        R"(##################..............)",  //  22
        R"(##################..............)",  //  23
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
