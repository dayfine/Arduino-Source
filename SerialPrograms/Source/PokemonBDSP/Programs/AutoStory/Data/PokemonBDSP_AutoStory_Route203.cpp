/*  BDSP AutoStory - Route 203 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Route203.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route203{

using namespace Pokemon;


//  Map source: Bulbapedia DP map image, traced by hand, then the east stretch
//      corrected on the console (r227 hand pass, 2026-10-31).
//  Console checks: gate warp column, ledge row 15, three sand rows, row 17
//      straight to Jubilife (2026-10-31).
//  Status: 01.06 passes on it (r230a). Next: re-derive the west and middle
//      stretches from the decomp (cells (23,6), (23,7)).
//
//  Traced by hand from Bulbapedia's Sinnoh_Route_203_DP.png (976 x 368 px,
//  16 px tiles from the top-left corner; three panels joined by stairs).
//  Checked on the console with D-pad taps (2026-10-31, r227 hand pass);
//  the east stretch's ledge and sand sat two rows too high in the trace.
//  -   West stretch (columns 0..21): the road on rows 17..19 runs to
//      Jubilife City at column 0; a fence closes row 16 west of the sign.
//  -   Middle stretch (24..35): the pond (29..35, 9..12) cuts the strip
//      north of it, by the item ball, off from the rest: r222g's walk got
//      stuck up there. The way through is along the bottom, rows 16..17.
//  -   East stretch (38..55): the Gate's mouth is (56, 9). South from
//      (55, 9) goes 9 tiles to row 18. The ledge on row 15 (columns 38..48)
//      is crossed southward only. The sand below it is three rows: the girl
//      (beaten) stands on row 16 at column 43.
//  -   Row 17 runs in one straight line west from the sand, over both
//      stairs, to Jubilife (35 taps, no row change). Row 16 also crosses the
//      east stairs (36..37) but ends at the blue-haired trainer (30, 16)
//      (r227b, r228a); row 18 ends at the stairs (r225a, r226a).
//  The blue-haired man near the Gate and the woman on the road (13, 17)
//  wander, so they are not on the map.
//  Trainers ('N') stand still; they are walls once beaten.
const TileGrid& grid(){
        //     0         1         2         3         4         5         6
        //     0123456789012345678901234567890123456789012345678901234567890
    static const TileGrid grid({
        R"(#############################################################)",  //   0
        R"(#############################################################)",  //   1
        R"(#############################################################)",  //   2
        R"(#############################################################)",  //   3
        R"(#############################################################)",  //   4
        R"(#############################################################)",  //   5
        R"(#############################.."""""..""""""""""""""#########)",  //   6
        R"(#####..............##########..#""""..""""""""""""""S...#####)",  //   7
        R"(#####..............##########.........""""""""""N"......#####)",  //   8
        R"(#####....##...####....##.....#########""""""""""""......D####)",  //   9
        R"(#####""""##...####....##..S..#########""""""""""""......#####)",  //  10
        R"(#####""""##...####....##.....#########"""""""""S""""""..#####)",  //  11
        R"(#####""""##.N.####..#.##.....#########"""""""""""##"""..#####)",  //  12
        R"(#####""""##...####....##....#####"""##....""...."##"""..#####)",  //  13
        R"(#####""""##...####vvvv##....#####"""##....""...."##"""..#####)",  //  14
        R"(#####"""".............##....#####"""##vvvvvvvvvvv"""""..#####)",  //  15
        R"(#####.S...............##......N..""".......N....""""""..#####)",  //  16
        R"(........................."""""..."""....................#####)",  //  17
        R"(........................##############..................#####)",  //  18
        R"(........................#####################################)",  //  19
        R"(#############################################################)",  //  20
        R"(#############################################################)",  //  21
        R"(#############################################################)",  //  22
    });
    return grid;
}

const std::vector<TilePos>& gate_exit(){
    static const std::vector<TilePos> tiles{{55, 9}, {54, 9}, {55, 8}, {55, 10}};
    return tiles;
}



}
}
}
}
