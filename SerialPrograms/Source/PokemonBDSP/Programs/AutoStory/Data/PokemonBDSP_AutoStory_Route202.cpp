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
//  Ledges: rows 9 and 18. On the DP map the south ledge looks like row 19,
//  but on the console the player stands at (22,19) right under it, beside
//  the trees at (21,19), and the sign at (24,20) is one row below.
//  The Poke Ball item at (3,6) is blocked.
//  NPCs stand on the tile under their feet; the sprites rise into the row
//  above. The girl at (22,15), pixel (365,235), is not the Potion giver;
//  that one is on Route 201.
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
        R"(###""""##vvvvv########vvvvvvvvv#####)",  //  18
        R"(###""""##.....########.........#####)",  //  19
        R"(###""""""......""""""...S......#####)",  //  20
        R"(###""""""......""""""..........#####)",  //  21
        R"(###""""""......""""""..........#####)",  //  22
        R"(#####################..........#####)",  //  23
        R"(#####################..........#####)",  //  24
        R"(#########################......#####)",  //  25
    });
    return grid;
}


//  Where the post-catch save stands: the bottom row of the tall-grass patch at
//  (15..20, 20..22), nearest the Sandgem Town entrance at the bottom right.
//  On the console the sign at (24,20) is 5 columns right and 2 rows up, and
//  walking east stops at the trees after 11 tiles.
const TilePos CATCH_SPOT{19, 22};

//  Blue-cap trainer, feet at about (128,186), facing south.
const TilePos TRAINER_A{7, 11};
//  Blue-cap trainer, feet at about (175,125), facing east.
const TilePos TRAINER_B{10, 7};
//  Blue-cap trainer, feet at about (430,77), facing south.
const TilePos TRAINER_C{26, 4};

//  Facing is read from the sprites. The range is a guess; DP trainers on
//  early routes see a few tiles.
const std::vector<TrainerSight>& trainer_sights(){
    static const std::vector<TrainerSight> sights{
        {"trainer_a", TRAINER_A, GridDirection::SOUTH, 4},
        {"trainer_b", TRAINER_B, GridDirection::EAST, 4},
        {"trainer_c", TRAINER_C, GridDirection::SOUTH, 4},
    };
    return sights;
}

//  Pixel (256,8): the path in the middle of the 8-tile gap at (11..18, 0).
const TilePos EXIT_JUBILIFE{15, 0};



}
}
}
}
