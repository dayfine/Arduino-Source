/*  BDSP AutoStory - Jubilife City tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_JubilifeCity.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace JubilifeCity{

using namespace Pokemon;


//  Read from Bulbapedia's DP map of Jubilife City (1024 x 772 px): 16 px tiles
//  from the top-left corner; the last 4 px rows are left out. Road and paved
//  plaza are walkable; buildings, roofs, trees, planters, lamps, signs, fences
//  and fountains are blocked. Tiles were classified by colour and only the
//  ones on the 00.05 route checked by eye, so some open plaza next to a lamp
//  or a pattern line may read as blocked; the planner goes around those, and
//  anything the map lacks is found by the route walker.
//  The DP layout matches BDSP (console B, 2026-10-28): the Trainers' School
//  and the Pokemon Center either side of the main road, the Poke Mart and the
//  fountain park north of them, the TV station at the top of the main road
//  and the Poketch Company in the north-west.
//  NPCs are the 00.05 story people, placed from the console: they stand still.
const TileGrid& grid(){
    //     0         1         2         3         4         5         6
    //     0123456789012345678901234567890123456789012345678901234567890123
    static const TileGrid grid({
        R"(################################################################)",  //   0
        R"(################################################################)",  //   1
        R"(##########################################......################)",  //   2
        R"(##########################################......################)",  //   3
        R"(##########################################......################)",  //   4
        R"(###########################...##########.......#################)",  //   5
        R"(###########################...##########........################)",  //   6
        R"(########.##########.#######...##########..#.....################)",  //   7
        R"(########.##########.#######...##########.......#################)",  //   8
        R"(########.##########.#######....#########..#....#################)",  //   9
        R"(########.##########.#######...#####D####..#....#################)",  //  10
        R"(########..#########.#######.......N.......#....#################)",  //  11
        R"(########............#######...............#....########....#####)",  //  12
        R"(########...##N......#######..........##...#....########....#####)",  //  13
        R"(######.#####.......#######.....................#######...#######)",  //  14
        R"(##........................................#....#...........#####)",  //  15
        R"(##...#..................................................#..#####)",  //  16
        R"(##...#...###..............###...........................#..#####)",  //  17
        R"(##...#..#####.################............#............#########)",  //  18
        R"(##############################..###########....#.###...#########)",  //  19
        R"(##############################..###########....######..#########)",  //  20
        R"(##############################..###########....#######..####.###)",  //  21
        R"(##############################..##########.....#################)",  //  22
        R"(##############################..###########....#......N#########)",  //  23
        R"(########.#####################..#.....##.......#........########)",  //  24
        R"(########.#####################.......####............#..####.###)",  //  25
        R"(########.#####.#########.######.##########..N.....###..#########)",  //  26
        R"(##############..########.######.###########....#######.#########)",  //  27
        R"(########.#####.############################....#################)",  //  28
        R"(########.#####.############################....#################)",  //  29
        R"(#################################.##.##D###....####D############)",  //  30
        R"(########.#####..#########.#####........................#########)",  //  31
        R"(#############....#######..####.####....................#####.###)",  //  32
        R"(#############....##.......########.....................#####.###)",  //  33
        R"(##############............##############...........N.N.#########)",  //  34
        R"(##############............##############...............#########)",  //  35
        R"(#############.............##############...........#############)",  //  36
        R"(########.#####.###...###.###############...........#############)",  //  37
        R"(########.#####.###...###.###############...........#############)",  //  38
        R"(########.#####.###...###.###############.#.....#...#############)",  //  39
        R"(########.#.#.#...........##########.....S..........#########.###)",  //  40
        R"(#########................##########................#########.###)",  //  41
        R"(########.........#.......##########................#########.###)",  //  42
        R"(########.#.#.#.#..........###############.......############.###)",  //  43
        R"(#########################################.........##############)",  //  44
        R"(###########################################....#################)",  //  45
        R"(###########################################DDDD#################)",  //  46
        R"(################################################################)",  //  47
    });
    return grid;
}

//  Checked on the console (2026-10-30, CFR "walk"): 00.05's save stands
//  below the steps, against the wall just east of them (the banner reads
//  Jubilife City). West along that wall does not stop at the steps (it runs
//  to the trees); a tile west, then north, climbs them. North up the main
//  road from the steps passes the lamps (41,39), (47,39) and does not stop
//  before the north exit. From the lamps' row, west stops at the house at
//  (35..39, 34..39) and east at the house at (51..55, 36..42); north from
//  (50,39) stops in front of the Pokemon Center. The road's white curbs
//  (columns 42 and 47) are not walls, as the colour reading had them.
const std::vector<TilePos>& up_the_steps(){
    static const std::vector<TilePos> tiles{{45, 43}, {46, 43}, {45, 44}, {46, 44}, {45, 45}, {46, 45}};
    return tiles;
}

const TilePos TRAINERS_SCHOOL_DOOR{39, 30};
const TilePos POKEMON_CENTER_DOOR{51, 30};
const TilePos TV_STATION_DOOR{35, 10};

//  On the main road just north of the school and the Center. Talked to from
//  the east, facing west (also works from the south).
const TilePos POKETCH_PRESIDENT{44, 26};
//  Below the Poke Mart's planters, where the road meets the plaza.
const TilePos CLOWN_MART{54, 23};
//  West of the TV station door.
const TilePos CLOWN_TV_STATION{34, 11};
//  East of the Poketch Company's sign. Talked to from the south on the
//  console; from the east a walk west along row 13 stops right beside him.
const TilePos CLOWN_POKETCH_COMPANY{13, 13};



}
}
}
}
