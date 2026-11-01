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


//  Map source: Bulbapedia DP map image, traced by hand (2026-10-28). The
//      image has 12 px rows for 16 px columns, so every row is 3/4 of a tile
//      (decomp comparison, 2026-10-31).
//  Console checks: north-east ledge row, Twinleaf and Sandgem gaps, the
//      Potion lady wandering (2026-10-28/29).
//  Status: 00.02 to 00.04 pass on it. Next: re-derive from the decomp (cells
//      (26,3), (26,4)), keep the console anchors.
//
//  Read from Bulbapedia's DP map of Route 201 (1040 x 320 px). As on the
//  Route 202 map, the tile grid starts 8 px in from the left edge: tile
//  (x, y) covers pixels [8 + 16x, 24 + 16x) x [16y, 16y + 16). The trees sit
//  about 3 px lower on their tiles than on the Route 202 map; tall grass and
//  the NPC at the Potion girl fit a grid with no vertical offset.
//
//  Trees follow the Route 202 rules: tiles under trunks or mostly under a
//  canopy are blocked. Ledges: row 10 west of the tall grass, and (53..56,7)
//  in the north-east. On the console the north-east ledge is one row higher
//  than it looks on the DP map, as on Route 202; the west one is unchecked.
//  West edge, rows 9..12: towards Lake Verity. South gap (14..17, 19):
//  Twinleaf Town. East edge, rows 3..6: Sandgem Town.
//  Route 201 has no trainers. Its NPCs wander, so they are left off: a
//  planned wall must stay put. The Poke Mart lady, who gives 10 Potions, is
//  one of them: she roams the north-east tall grass (42..52, 3) and the open
//  ground under the trees above it, and eight runs (r113a..r120a) never got
//  her talk, so 00.04 buys its Potions in Sandgem instead.
const TileGrid& grid(){
    //     0         1         2         3         4         5         6
    //     0123456789012345678901234567890123456789012345678901234567890123
    static const TileGrid grid({
        R"(################################################################)",  //   0
        R"(################################################################)",  //   1
        R"(################################################################)",  //   2
        R"(##########################################"""""""""""...........)",  //   3
        R"(##########################################"""""""""""..S........)",  //   4
        R"(##########################################"""""""""##...........)",  //   5
        R"(##..####..####....######..##....##..####.."""""""""##...........)",  //   6
        R"(##..####..####....######..##....##..####.."""""""""##vvvv.######)",  //   7
        R"(##........####............................"""""""""##.....######)",  //   8
        R"(....####.................................."""""""""##.....######)",  //   9
        R"(vvvv####vvvvvvvvvvvvvvvvvvvvvvvv........S."""""""""##.....######)",  //  10
        R"(....####............""""""""""............................######)",  //  11
        R"(....................""""""""""............................######)",  //  12
        R"(########.....S......##########............................######)",  //  13
        R"(##############....############################............######)",  //  14
        R"(##############....############################""""""""""""######)",  //  15
        R"(##############....##############################################)",  //  16
        R"(##############....##############################################)",  //  17
        R"(##############....##############################################)",  //  18
        R"(##############....##############################################)",  //  19
    });
    return grid;
}


//  Pixel (888,64) on the DP map, on the doors' row of Sandgem (world row 4):
//  walking east along row 4 from the grass stops at (54,4), and facing east
//  from there reads it.
//  Checked on the console (2026-10-30, CFR "walk"): a row south of her row
//  (row 3), east stops at the sign's west side and A reads "Route 201 ->
//  Sandgem Town" (a green box); north from there is one row, to row 3. Row 3
//  runs on east past the sign's north side to the Lab's fence. Row 5, the
//  path, runs from the grass's west edge to Sandgem without a stop.
const TilePos BORDER_SIGN{55, 4};
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
