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


//  Map source: the decomp (pret/pokediamond land_data narc_0005 and
//      narc_0006, 2026-10-31) with the objects from its events file;
//      Platinum's map_data_005/006 have the same terrain.
//  Console checks: the border sign read from (54,4) facing east, row 3 open
//      east of the grass to Sandgem's fence, row 5 to Sandgem without a stop,
//      the Potion lady on row 3, wandering (2026-10-28 to 30, CFR "walk").
//  Status: no checkpoint has run on this grid yet. 00.02 to 00.04 and the
//      mart_lady checkpoint passed on the image-traced grid it replaces (3/4
//      of a tile per row) and must be re-run.
//
//  Decoded from Pokemon Diamond's collision table: cells (26,3) and (26,4)
//  of the Sinnoh matrix, with the objects from its events file. Local (x, y)
//  = world (x - 96, z - 839); 64 x 25 tiles, rows 0..2 are padding. The
//  columns are the ones the image-traced grid had (its anchors matched the
//  decomp exactly); the rows are the game's: the image gave 20 rows for 25.
//  Sandgem Town's grid joins at column 64, row -3 (its front row is row 4).
//
//  Edges: west, rows 6..7 and 11 (Lake Verity, off the map); south, the
//  Twinleaf gap (14..17, 21..24); east, rows 3..6, into Sandgem. Ledges:
//  row 12 (0..3 and 8..31) and (53..57, 9) in the north-east; both are
//  jumped southward only. The north-east grass (42..52, 3..6) is four rows
//  deep and its tree stub (51..52) stands on rows 7..12; the middle grass
//  (20..29, 13..16) four rows; the south-east grass (42..57, 18) and
//  (46..57, 19..20).
//  Objects, from the events file: the signs (55,4) to Sandgem (BORDER_SIGN),
//  (13,17) to Twinleaf and the Trainer Tips sign (40,12); a School Kid who
//  stands at (57,8) looking west (not yet seen on the console). Route 201
//  has no trainers and no items on the ground. Wanderers, left off the grid:
//  a Breeder around (27,15), columns 26..28, rows 14..16; a Lass along row
//  16, columns 44..48. The Poke Mart lady, who gives 10 Potions, stands still
//  at (44,3) in Diamond but wanders in BDSP (eight runs, r113a..r120a, never
//  got her talk), so she is left off too and 00.04 buys its Potions in
//  Sandgem. Rowan, Barry and the briefcase by the lake are hidden by story
//  flags and left off.
const TileGrid& grid(){
    //     0         1         2         3         4         5         6
    //     0123456789012345678901234567890123456789012345678901234567890123
    static const TileGrid grid({
        R"(################################################################)",  //   0
        R"(################################################################)",  //   1
        R"(################################################################)",  //   2
        R"(##########################################"""""""""""...........)",  //   3
        R"(##########################################"""""""""""..S........)",  //   4
        R"(##########################################"""""""""""...........)",  //   5
        R"(##########################################"""""""""""...........)",  //   6
        R"(....####..........######..##....##..####.."""""""""##...........)",  //   7
        R"(....####..........######..##....##..####.."""""""""##....N......)",  //   8
        R"(##........####............................"""""""""##vvvvv######)",  //   9
        R"(##........####............................"""""""""##.....######)",  //  10
        R"(.........................................."""""""""##.....######)",  //  11
        R"(vvvv####vvvvvvvvvvvvvvvvvvvvvvvv........S."""""""""##.....######)",  //  12
        R"(....####............""""""""""............................######)",  //  13
        R"(....................""""""""""............................######)",  //  14
        R"(....................""""""""""............................######)",  //  15
        R"(....................""""""""""............................######)",  //  16
        R"(########.....S......##########............................######)",  //  17
        R"(########............##########............""""""""""""""""######)",  //  18
        R"(##############....############################""""""""""""######)",  //  19
        R"(##############....############################""""""""""""######)",  //  20
        R"(##############....##############################################)",  //  21
        R"(##############....##############################################)",  //  22
        R"(##############....##############################################)",  //  23
        R"(##############....##############################################)",  //  24
    });
    return grid;
}


//  The arrow sign to Sandgem Town, world (151,843), on the row of Sandgem's
//  doors (its row 7). Checked on the console (2026-10-30, CFR "walk"): a row
//  south of the lady's row (row 3), east stops at the sign's west side and A
//  reads "Route 201 -> Sandgem Town" (a green box); north from there is one
//  row, to row 3. Row 3 runs on east past the sign's north side to the Lab's
//  fence. Row 5, the path, runs from the grass's west edge to Sandgem without
//  a stop.
const TilePos BORDER_SIGN{55, 4};
//  Her spawn tile in Diamond is (44,3), the east end of the top grass row.
//  On the console (by hand, then through the Lab Console; 99715d5d6 and
//  f823453e9 on bdsp/new-game-intro-v2) she stood at (43,3), and walking
//  west along row 3 from the trees at its east end stopped at (44,3), facing
//  her; her lines need one A each, 1.5 s apart. Later runs (r113a..r120a)
//  found her wandering.
//  Checked on the console (2026-10-30, CFR "walk"), twice: from the grass's
//  west edge on row 5, north stops at the trees 2 rows up, and east from
//  there stops at her. r135a missed her walking west along row 4.
const TilePos POTION_GIVER{43, 3};
const TilePos POTION_GIVER_TALK_FROM{44, 3};



}
}
}
}
