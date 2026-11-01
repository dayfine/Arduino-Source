/*  BDSP AutoStory - Sandgem Town tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_SandgemTown.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace SandgemTown{

using namespace Pokemon;


//  Map source: rows 0 to 6 from Bulbapedia's DP map image; rows 7 and on from
//      the decomp (map_data_007), signs and doors from its events.
//  Console checks: all three doors, five signs, the north road stop at (29,2)
//      (2026-10-29).
//  Status: 00.04 and 00.05 pass on it. Next: open the Lab-Center gap on row 6
//      ((11,6), (14,6)); re-check where the north road narrows (row 2 or 3).
//
//  Read from Bulbapedia's DP map of Sandgem Town (Sandgem_Town_DP.png,
//  560 x 368 px). The tile grid starts 8 px in from the left edge, as on the
//  route maps: tile (x, y) covers pixels [8 + 16x, 24 + 16x) x [16y, 16y + 16).
//
//  Buildings are blocked; each door is the tile the player walks into, on
//  the building's front row. Top left: Professor Rowan's lab, with a fence
//  running west along row 6 to the map edge. Top middle: Pokemon Center.
//  Top right: Poke Mart. Exits: west, rows 7..11 (Route 201); north,
//  (20..29, 0) (Route 202); south, (20..25, 27) (the beach).
//
//  Rows 7 and on are the game's own collision, not the map image: Platinum's
//  land data for Sandgem (pret/pokeplatinum res/field/maps/data/
//  map_data_007.bin, its permissions' collision bit; grid row = the map's
//  row - 4, column = the map's column), with the signs and doors from
//  res/field/events/events_sandgem_town.json. The image squeezed rows 7..18
//  into 7..15: it put both houses' doors on row 15, where the game has them
//  on row 18, and the east house's wall at rows 10..14 where the game has
//  open ground. r123a planned a west leg to that wall on row ~11, ran on
//  past it and entered the west house's door going north. The townspeople
//  south of the Mart (a youngster and two breeders) wander: not marked.
//
//  Checked on the console: walking south from Route 202 (30,22) stops at
//  (29,2) above the Poke Mart, and walking west from there stops at (20,2).
//  The lab's sign stands on row 7, in front of the fence: walking west along
//  row 7 stops at it.
const TileGrid& grid(){
    //     0         1         2         3
    //     0123456789012345678901234567890123
    static const TileGrid grid({
        R"(####################..........####)",  //   0
        R"(####################..........####)",  //   1
        R"(####################..........####)",  //   2
        R"(####################......########)",  //   3
        R"(####################......########)",  //   4
        R"(####################......########)",  //   5
        R"(########D###..###D##S....S#D######)",  //   6
        R"(.....S........................####)",  //   7
        R"(..............................####)",  //   8
        R"(..............................####)",  //   9
        R"(..............................####)",  //  10
        R"(..............................####)",  //  11
        R"(######........................####)",  //  12
        R"(######........................####)",  //  13
        R"(######.........#####..........####)",  //  14
        R"(##########.....#####..........####)",  //  15
        R"(##########.....#####..........####)",  //  16
        R"(##########.....#####..........####)",  //  17
        R"(#######D##....S#D###..S.......####)",  //  18
        R"(######........................####)",  //  19
        R"(######........................####)",  //  20
        R"(######........................####)",  //  21
        R"(######....................########)",  //  22
        R"(####################......########)",  //  23
        R"(####################......########)",  //  24
        R"(####################......########)",  //  25
        R"(####################......########)",  //  26
        R"(####################......########)",  //  27
    });
    return grid;
}


//  Pixel (144,110): the lab's door, between its two front windows.
const TilePos LAB_DOOR{8, 6};
//  Pixel (288,104): the Pokemon Center's glass door.
const TilePos POKEMON_CENTER_DOOR{17, 6};
//  On the console, walking south from (20,2) stops at (20,5), above it.
const TilePos POKEMON_CENTER_SIGN{20, 6};
//  Pixel (448,104): the Poke Mart's door.
const TilePos POKE_MART_DOOR{27, 6};
//  Walking east along the doors' front row stops here, against the trees.
//  Checked on the console (2026-10-30, CFR "walk"): east along row 8 (the
//  path under the Lab's sign) from Route 201 stops at (29,8), and north from
//  there stops here. From below the Center's sign (20,7), west stops at the
//  Lab's sign, which reads facing west; south from there stops at the house
//  4 rows down, and north back at the sign's row; west of the house's north
//  side is trees. A 200 ms hold south took one row; 300 ms took one or two.
const TilePos FRONT_ROW_EAST_END{29, 7};

std::vector<TilePos> below_sign_gap(){
    return {{21, 7}, {22, 7}, {23, 7}, {24, 7}};
}



}
}
}
}
