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


//  Read from Bulbapedia's DP map of Sandgem Town (Sandgem_Town_DP.png,
//  560 x 368 px). The tile grid starts 8 px in from the left edge, as on the
//  route maps: tile (x, y) covers pixels [8 + 16x, 24 + 16x) x [16y, 16y + 16).
//
//  Buildings are blocked; each door is the tile the player walks into, on
//  the building's front row. Top left: Professor Rowan's lab, with a fence
//  running west along row 6 to the map edge. Top middle: Pokemon Center.
//  Top right: Poke Mart. Exits: west, rows 7..9 (Route 201); north,
//  (20..29, 0) (Route 202); south, (20..25, 22) (the beach).
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
        R"(######.........#####..........####)",  //  10
        R"(######.........#####..........####)",  //  11
        R"(##########.....#####..........####)",  //  12
        R"(##########.....#####..........####)",  //  13
        R"(##########.....#####..........####)",  //  14
        R"(#######D##....S#D###..S.......####)",  //  15
        R"(######........................####)",  //  16
        R"(######........................####)",  //  17
        R"(######....................########)",  //  18
        R"(####################......########)",  //  19
        R"(####################......########)",  //  20
        R"(####################......########)",  //  21
        R"(####################......########)",  //  22
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



}
}
}
}
