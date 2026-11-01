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


//  Map source: the decomp (pret/pokediamond land_data narc_0314, matrix 006;
//      2026-10-31) with the objects from its events file; Platinum's
//      map_data_337 is the same.
//  Console checks (2026-10-28): the corridor's rocks zigzag a row at a time,
//      the room's second row passes under the worker on the first, Roark
//      stands a row above the bottom row between two rocks; the walk north
//      from the corridor's corner reached the entrance hall (r99).
//  Status: no checkpoint has run on this grid yet. 01.02 and 01.03 passed
//      3x on the stretched image grid it replaces, which had every row one
//      lower, the top corridor a row short, the room's second row blocked
//      and no ledge in the west corridor; re-run both.
//
//  Decoded from Pokemon Diamond's collision table: the one cell of matrix
//  006, with the objects from its events file. Local (x, y) = world (x, z);
//  32 x 32 tiles. The way up to the entrance hall is the warp tile (15,1),
//  marked 'D': stepping onto it leaves the room. The console walked north
//  into the hall from column 14 (r99), where this table has the way up only
//  at column 15: a stick hold drifts, or BDSP's stairs are wider.
//  The corridor from the top (13..17, 2..6) zigzags down to the room's first
//  row (15); the room's second row (17) is open from 2 to 29 and passes
//  under the worker and his Machop on the first; the coal block fills
//  (7..24, 18..25) with the corridors either side; the west corridor has a
//  ledge (2..4, 19), crossed southward only; the bottom rows 28..29 run
//  under the block.
//  Objects, from the events file: Roark (18,28), between two rocks (17,28)
//  and (19,28); workers who stand still at (8,28) looking north, (26,20)
//  looking west and (21,15) with Machop at (20,15) and (27,15); Poke Balls
//  at (4,28) and (26,24), walls until picked up. A Machop wanders along row
//  28, columns 11..13, and is left off the grid.
const TileGrid& grid(){
    //     0         1         2         3
    //     01234567890123456789012345678901
    static const TileGrid grid({
        R"(################################)",  //   0
        R"(###############D################)",  //   1
        R"(#############.....##############)",  //   2
        R"(#############.....##############)",  //   3
        R"(#############.....##############)",  //   4
        R"(#############.....##############)",  //   5
        R"(#############.....##############)",  //   6
        R"(#############...################)",  //   7
        R"(#############...################)",  //   8
        R"(#############.....##############)",  //   9
        R"(#############..#..##############)",  //  10
        R"(###############...##############)",  //  11
        R"(#############...################)",  //  12
        R"(#############.....##############)",  //  13
        R"(###############...##############)",  //  14
        R"(########...####.....NN.....N.###)",  //  15
        R"(######.....##.................##)",  //  16
        R"(##.#..........................##)",  //  17
        R"(##....####################....##)",  //  18
        R"(##vvv##..................#....##)",  //  19
        R"(##....#..................#N...##)",  //  20
        R"(##.##.#..................#....##)",  //  21
        R"(##..###..................###..##)",  //  22
        R"(##..###..................###..##)",  //  23
        R"(##....#..................##...##)",  //  24
        R"(##.##.#..................#....##)",  //  25
        R"(####..####################....##)",  //  26
        R"(####..####################....##)",  //  27
        R"(#####...N........#N#........####)",  //  28
        R"(######......................####)",  //  29
        R"(################################)",  //  30
        R"(################################)",  //  31
    });
    return grid;
}

//  The first open row below the warp tile, where the walk in from the hall
//  leaves us (or the row below: the plan's first legs run to walls and meet).
const std::vector<TilePos>& corridor_top(){
    static const std::vector<TilePos> tiles{{14, 2}, {15, 2}, {16, 2}};
    return tiles;
}

const TilePos ROARK{18, 28};
const TilePos CORRIDOR_EXIT_START{14, 10};


}
}
}
}
