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


//  Map source: the decomp (pret/pokediamond land_data narc_0007, 2026-10-31)
//      with the signs and doors from its events file; Platinum's map_data_007
//      is the same. The rows from the doors down have been the decomp's since
//      2026-10-29; the four rows above the doors were the image's, and the
//      grid now starts where the game's map does, four rows higher.
//  Console checks: all three doors, five signs, the row below the doors east
//      to the trees and north from it, the Lab's sign read facing west, the
//      walk south from Route 202 stopping above the Mart (2026-10-29/30).
//  Status: no checkpoint has run on this grid yet. 00.04, 00.05 and the
//      Sandgem Center and Mart lady checkpoints passed with the image's rows
//      above the doors; changed since: every tile is 4 rows lower (the grid
//      begins at the map's edge), the road from Route 202 narrows on row 6
//      (the walk south from Route 202 stops at (29,5)), and the gap between
//      the Lab's fence and the Center on the doors' row is four wide (11..14).
//      Re-run them all.
//
//  Decoded from Pokemon Diamond's collision table: cell (26,5) of the Sinnoh
//  matrix, with the signs and doors from its events file. Local (x, y) =
//  world (x - 160, z - 832); 34 x 32 tiles, the whole cell, so the grid's
//  edges are where the game changes its location banner. Route 201 joins at
//  column -64, row 7 (its row 4 is this grid's row 11); Route 202's road
//  comes down onto row 0 at columns 24..29.
//
//  Buildings are blocked; each door is the tile the player walks into, on
//  the building's front row (row 10). Top left: Professor Rowan's lab, with a
//  fence running west along row 10 to the map edge. Top middle: Pokemon
//  Center. Top right: Poke Mart. Exits: west, rows 11..15 (Route 201); north,
//  (24..29, 0) (Route 202: the road is 6 wide on rows 0..1, 10 wide on rows
//  2..5 and 6 wide again, columns 20..25, on rows 6..9); south, (20..25, 31)
//  (the beach).
//
//  The image squeezed the rows below the doors: it put both houses' doors
//  three rows too high and the east house's wall where the game has open
//  ground; r123a planned a west leg to that wall, ran on past it and entered
//  the west house's door going north. The townspeople south of the Mart (a
//  youngster and two breeders, who stand still in Diamond at (11,22) and
//  (23,20)) wander on the console: not marked. Rowan, Barry and Dawn are
//  hidden by story flags.
//
//  Checked on the console: walking south from Route 202 along its column 30
//  stops above the Poke Mart, where the road narrows (counted as two rows
//  above the signs' row on the image grid; the decomp has the wall at (29,6),
//  so the stop is (29,5)), and walking west from there stops at (20,5). The
//  lab's sign stands on row 11, in front of the fence: walking west along
//  row 11 stops at it.
const TileGrid& grid(){
    //     0         1         2         3
    //     0123456789012345678901234567890123
    static const TileGrid grid({
        R"(########################......####)",  //   0
        R"(########################......####)",  //   1
        R"(####################..........####)",  //   2
        R"(####################..........####)",  //   3
        R"(####################..........####)",  //   4
        R"(####################..........####)",  //   5
        R"(####################......########)",  //   6
        R"(####################......########)",  //   7
        R"(####################......########)",  //   8
        R"(####################......########)",  //   9
        R"(########D##....##D##S....S#D######)",  //  10
        R"(.....S........................####)",  //  11
        R"(..............................####)",  //  12
        R"(..............................####)",  //  13
        R"(..............................####)",  //  14
        R"(..............................####)",  //  15
        R"(######........................####)",  //  16
        R"(######........................####)",  //  17
        R"(######.........#####..........####)",  //  18
        R"(##########.....#####..........####)",  //  19
        R"(##########.....#####..........####)",  //  20
        R"(##########.....#####..........####)",  //  21
        R"(#######D##....S#D###..S.......####)",  //  22
        R"(######........................####)",  //  23
        R"(######........................####)",  //  24
        R"(######........................####)",  //  25
        R"(######....................########)",  //  26
        R"(####################......########)",  //  27
        R"(####################......########)",  //  28
        R"(####################......########)",  //  29
        R"(####################......########)",  //  30
        R"(####################......########)",  //  31
    });
    return grid;
}


const TilePos ROUTE_202_ENTRY_STOP{29, 5};
//  The lab's door, between its two front windows.
const TilePos LAB_DOOR{8, 10};
//  The Pokemon Center's glass door.
const TilePos POKEMON_CENTER_DOOR{17, 10};
//  On the console, walking south along the road's west edge above it stops
//  at (20,9), above it.
const TilePos POKEMON_CENTER_SIGN{20, 10};
//  The Poke Mart's door.
const TilePos POKE_MART_DOOR{27, 10};
//  Walking east along the doors' front row stops here, against the trees.
//  Checked on the console (2026-10-30, CFR "walk"): east along row 12 (the
//  path under the Lab's sign) from Route 201 stops at (29,12), and north from
//  there stops here. From below the Center's sign (20,11), west stops at the
//  Lab's sign, which reads facing west; south from there stops at the house
//  4 rows down, and north back at the sign's row; west of the house's north
//  side is trees. A 200 ms hold south took one row; 300 ms took one or two.
const TilePos FRONT_ROW_EAST_END{29, 11};

std::vector<TilePos> below_sign_gap(){
    return {{21, 11}, {22, 11}, {23, 11}, {24, 11}};
}



}
}
}
}
