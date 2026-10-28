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
//  Route 201 has no trainers; the N tiles are NPCs.
const TileGrid& grid(){
    //     0         1         2         3         4         5         6
    //     0123456789012345678901234567890123456789012345678901234567890123
    static const TileGrid grid({
        R"(################################################################)",  //   0
        R"(################################################################)",  //   1
        R"(################################################################)",  //   2
        R"(##########################################""N""""""""...........)",  //   3
        R"(##########################################"""""""""""..S........)",  //   4
        R"(##########################################"""""""""##...........)",  //   5
        R"(##..####..####....######..##....##..####.."""""""""##...........)",  //   6
        R"(##..####..####....######..##....##..####.."""""""""##vvvvN######)",  //   7
        R"(##........####............................"""""""""##.....######)",  //   8
        R"(....####.................................."""""""""##.....######)",  //   9
        R"(vvvv####vvvvvvvvvvvvvvvvvvvvvvvv........S."""""""""##.....######)",  //  10
        R"(....####............""""""N"""............................######)",  //  11
        R"(....................""""""""""............................######)",  //  12
        R"(########.....S......##########..................N.........######)",  //  13
        R"(##############....############################............######)",  //  14
        R"(##############....############################""""""""""""######)",  //  15
        R"(##############....##############################################)",  //  16
        R"(##############....##############################################)",  //  17
        R"(##############....##############################################)",  //  18
        R"(##############....##############################################)",  //  19
    });
    return grid;
}


//  Pixel (718,45) is the sprite's middle; its feet are at about (718,57),
//  on the top row of the tall grass with the tree trunks right above.
const TilePos POTION_GIVER{44, 3};
const TilePos POTION_GIVER_TALK_SPOT{45, 3};



}
}
}
}
