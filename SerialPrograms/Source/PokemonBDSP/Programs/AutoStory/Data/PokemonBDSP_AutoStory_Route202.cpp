/*  BDSP AutoStory - Route 202 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Route202.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route202{

using namespace Pokemon;


//  Read from Bulbapedia's DP map of Route 202 (592 x 416 px). Its tile grid
//  starts 8 px in from the left edge, so tile (x, y) covers pixels
//  [8 + 16x, 24 + 16x) x [16y, 16y + 16). The half tiles at both side edges
//  are trees and are left out.
//
//  Trees: each tile under a trunk or a canopy that hides most of the ground
//  is blocked. The thin top of a canopy that only overhangs the row above a
//  tree is not.
//  Ledges: rows 9 and 19; the one at (24,19) is behind the sign.
//  The Poke Ball item at (3,6) is blocked.
//  NPCs stand on the tile under their feet; the sprites rise into the row
//  above.
const TileGrid& grid(){
    //     0         1         2         3
    //     012345678901234567890123456789012345
    static const TileGrid grid({
        R"(###########........#################)",  //   0
        R"(###########........#################)",  //   1
        R"(###########........#################)",  //   2
        R"(###########........#################)",  //   3
        R"(###""""""........S..""""".N....#####)",  //   4
        R"(###""""""..........."""""......#####)",  //   5
        R"(####"""""..........."""""......#####)",  //   6
        R"(###"""""".N...########"""......#####)",  //   7
        R"(#########.....########.........#####)",  //   8
        R"(#########vvvvv########vvvvvv...""###)",  //   9
        R"(#######.......########"""""""""""###)",  //  10
        R"(#######N..........####"""""""""""###)",  //  11
        R"(###...............####"""""""""""###)",  //  12
        R"(###............S..####"""""""""""###)",  //  13
        R"(###...............####"""""""""""###)",  //  14
        R"(###....##.........####N."""""""""###)",  //  15
        R"(###""""##..............."""""""""###)",  //  16
        R"(###""""##....................""#####)",  //  17
        R"(###""""##.....########.........#####)",  //  18
        R"(###""""##vvvvv########vvvvvvvvv#####)",  //  19
        R"(###""""""......""""""...S......#####)",  //  20
        R"(###""""""......""""""..........#####)",  //  21
        R"(###""""""......""""""..........#####)",  //  22
        R"(#####################..........#####)",  //  23
        R"(#####################..........#####)",  //  24
        R"(#########################......#####)",  //  25
    });
    return grid;
}


//  Pixel (304,344): middle of the tall-grass patch at (15..20, 20..22),
//  the patch nearest the Sandgem Town entrance at the bottom right.
const TilePos CATCH_SPOT{18, 21};

//  Pixel (365,235) is the sprite's middle; its feet are at about (370,248).
//  She stands east of the central trees' trunks, south of the tall grass.
const TilePos POTION_GIVER{22, 15};
const TilePos POTION_GIVER_FRONT{22, 16};

//  Blue-cap trainer, feet at about (128,186), facing south.
const TilePos TRAINER_TRISTAN{7, 11};
//  Blue-cap trainer, feet at about (175,125), facing east.
const TilePos TRAINER_NATALIE{10, 7};
//  Blue-cap trainer, feet at about (430,77), facing south.
const TilePos TRAINER_LOGAN{26, 4};

//  Pixel (256,8): the path in the middle of the 8-tile gap at (11..18, 0).
const TilePos EXIT_JUBILIFE{15, 0};



}
}
}
}
