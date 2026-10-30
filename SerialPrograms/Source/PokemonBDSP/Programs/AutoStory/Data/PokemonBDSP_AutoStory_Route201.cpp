/*  BDSP AutoStory - Route 201 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Route201.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route201{

using namespace Pokemon;


//  Read from Bulbapedia's DP map of Route 201 (1040 x 320 px). As on the
//  Route 202 map, the tile grid starts 8 px in from the left edge: tile
//  (x, y) covers pixels [8 + 16x, 24 + 16x) x [16y, 16y + 16). The trees sit
//  about 3 px lower on their tiles than on the Route 202 map; tall grass and
//  the NPC at the Potion girl fit a grid with no vertical offset.
//
//  Trees follow the Route 202 rules: tiles under trunks or mostly under a
//  canopy are blocked. Ledges: row 10 west of the tall grass, and (53..56,7)
//  in the north-east. On the console the north-east ledge is one row higher
//  than it looks on the DP map, as on Route 202; the west one is unchecked. West edge, rows 9..12: towards Lake Verity. South gap
//  (14..17, 19): Twinleaf Town. East edge, rows 3..6: Sandgem Town.
//  Route 201 has no trainers. Its other NPCs wander, so they are left off:
//  a planned wall must stay put. The Potion giver moves too (see below).
const TileGrid& grid(){
    //     0         1         2         3         4         5         6
    //     0123456789012345678901234567890123456789012345678901234567890123
    static const TileGrid grid({
        R"(################################################################)",  //   0
        R"(################################################################)",  //   1
        R"(################################################################)",  //   2
        R"(##########################################"""""""""""...........)",  //   3
        R"(##########################################"""""""""""..S........)",  //   4
        R"(##########################################"""""""""##...........)",  //   5
        R"(##..####..####....######..##....##..####.."""""""""##...........)",  //   6
        R"(##..####..####....######..##....##..####.."""""""""##vvvv.######)",  //   7
        R"(##........####............................"""""""""##.....######)",  //   8
        R"(....####.................................."""""""""##.....######)",  //   9
        R"(vvvv####vvvvvvvvvvvvvvvvvvvvvvvv........S."""""""""##.....######)",  //  10
        R"(....####............""""""""""............................######)",  //  11
        R"(....................""""""""""............................######)",  //  12
        R"(########.....S......##########............................######)",  //  13
        R"(##############....############################............######)",  //  14
        R"(##############....############################""""""""""""######)",  //  15
        R"(##############....##############################################)",  //  16
        R"(##############....##############################################)",  //  17
        R"(##############....##############################################)",  //  18
        R"(##############....##############################################)",  //  19
    });
    return grid;
}


//  On the DP map she is at (44,3), pixel (718,57), and she was once met at
//  (43,3) on the console. She does not stay there: r113a and r114a walked
//  west along row 3 to its west end (42,3) through her tile, and found her
//  one tile east and one row up (r114a), or two tiles east in the row
//  (r113a). So she is left off the grid like the other NPCs, and is found by
//  walking east from the row's west end until she answers.
const TilePos POTION_GIVER_ROW_EAST{53, 3};
const TilePos POTION_GIVER_ROW_WEST_END{42, 3};
const TilePos POTION_GIVER_ROW_EAST_END{52, 3};



}
}
}
}
