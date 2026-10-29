/*  BDSP AutoStory - Oreburgh Mine tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_OreburghMine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace OreburghMine{

using namespace Pokemon;


//  Read from Bulbapedia's DP map Oreburgh_Mine_B2F_DP.png with
//  devtools/maps/levels.py (--profile cave --height 512 --offset 0,4
//  --objects on --edits oreburgh_mine_b2f_edits.txt). The archive image is
//  squashed to 512x395; stretched back to 512x512 its rocks sit on 16 px
//  tiles both ways. Rocks, boulders, crates, the coal block, the workers and
//  their Machop are blocked. Checked on the console (2026-10-28): the
//  corridor's small rocks zigzag a row at a time; the coal block's top edge
//  is drawn over the room's second row, which is walkable and is how to pass
//  the worker on the first row; Roark stands a row above the bottom row.
const TileGrid& grid(){
    static const TileGrid grid({
    //     0         1         2         3
        R"(################################)",  //   0
        R"(################################)",  //   1
        R"(################################)",  //   2
        R"(###############.################)",  //   3
        R"(#############.....##############)",  //   4
        R"(#############.....##############)",  //   5
        R"(#############.....##############)",  //   6
        R"(#############.....##############)",  //   7
        R"(#############...################)",  //   8
        R"(#############...################)",  //   9
        R"(#############.....##############)",  //  10
        R"(#############..#..##############)",  //  11
        R"(###############...##############)",  //  12
        R"(#############...################)",  //  13
        R"(#############.....##############)",  //  14
        R"(###############...##############)",  //  15
        R"(########...####.....##.....#.###)",  //  16
        R"(##............................##)",  //  17
        R"(##.#..###################.....##)",  //  18
        R"(##....###################.....##)",  //  19
        R"(##...######################...##)",  //  20
        R"(#####.#####################...##)",  //  21
        R"(##.########################...##)",  //  22
        R"(##..########################..##)",  //  23
        R"(##..########################..##)",  //  24
        R"(##....#####################...##)",  //  25
        R"(#####.#####################...##)",  //  26
        R"(####..####################....##)",  //  27
        R"(####..####################..####)",  //  28
        R"(####....#..#.....#N#........####)",  //  29
        R"(######......................####)",  //  30
        R"(################################)",  //  31
    });
    return grid;
}

const std::vector<TilePos>& corridor_top(){
    static const std::vector<TilePos> tiles{{14, 4}, {15, 4}, {16, 4}};
    return tiles;
}

const TilePos ROARK{18, 29};
const TilePos CORRIDOR_EXIT_START{14, 11};


}
}
}
}
