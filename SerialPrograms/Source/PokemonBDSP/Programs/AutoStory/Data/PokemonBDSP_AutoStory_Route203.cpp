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


//  Map source: the decomp (pret/pokediamond land_data narc_0019 and
//      narc_0020, with Jubilife's column 0 from narc_0016; 2026-10-31) and
//      the objects from its events file. Platinum adds an X Defense ball at
//      (40,14).
//  Console checks (r227 hand pass, 2026-10-31): the Gate's warp at column
//      55, the ledge on row 15 (38..48), the beaten girl at column 43 on the
//      second sand row, the row below her straight west to Jubilife (35
//      taps, no row change), the blue-haired trainer at the sand's west end.
//  Status: no checkpoint has run on this grid yet. 01.06 passed on the
//      image-traced grid it replaces (its east stretch corrected on the
//      console) and must be re-run: the decomp has four sand rows under the
//      ledge where the console counted three (9 taps south from (55,9); the
//      decomp has 10 open tiles, so the first tap may only have turned).
//
//  Decoded from Pokemon Diamond's collision table: cells (23,6) and (23,7)
//  of the Sinnoh matrix, with the objects from its events file. Local
//  (x, y) = world (x - 191, z - 740); 61 x 24 tiles. Column 0 is Jubilife's
//  east edge. The columns are the ones the image-traced grid had (its gate
//  warp, ledge and signs matched the decomp exactly); the rows are the
//  game's: the image squeezed the west and middle stretches.
//  -   West stretch (columns 5..22): the field (5..22, 4..19) with grass at
//      5..8 (8..13), a ledge (18..22, 13), and the road on rows 17..19 that
//      runs to Jubilife at column 0.
//  -   Middle stretch (23..37): the pond (29..34, 7..11) cuts the strip north
//      of it, by the Repel ball (31,5), off from the rest: r222g's walk got
//      stuck up there. The way through is along the bottom, rows 16..19.
//  -   East stretch (38..55): the Gate's mouth is (56,9), its warp tile
//      (55,9). South from (55,9) is open to row 19. The ledge on row 15
//      (38..48) is crossed southward only. The sand below it is four rows,
//      16..19: the blue-haired trainer (Youngster Dallas, looks south, sees
//      4) at the sand's west end on its first row, the girl (Youngster
//      Sebastian in the events file, looks west, sees 5) at (43,17) on its
//      second.
//  Row 18 runs in one straight line west from the sand to Jubilife; row 17
//  is blocked by the girl and the trainer, row 19 by trees at (36..37, 19).
//  Objects, from the events file: trainers who stand still, 'N': Youngster
//  Michael (13,10), looks west, sees 2; Dallas (31,16); Sebastian (43,17);
//  Lass Madeline (49,8), looks south, sees 5. Items that block their tile
//  until picked up, '#': a Poke Ball (20,10) and a Repel (31,5). Signs: to
//  Jubilife (9,16), Trainer Tips (26,10) and (47,12), to Oreburgh (53,7);
//  Jubilife's own sign to Route 203 stands at (7,16) in its events, not in
//  this map's. Left off: a Lass who wanders along row 17 (14..16) and the
//  Youngster at (54,16) who stands still in Diamond but wanders in BDSP (the
//  blue-haired man near the Gate, on the console); Barry (10,17) is hidden
//  by a story flag.
const TileGrid& grid(){
    //     0         1         2         3         4         5         6
    //     0123456789012345678901234567890123456789012345678901234567890
    static const TileGrid grid({
        R"(#############################################################)",  //   0
        R"(#############################################################)",  //   1
        R"(#############################################################)",  //   2
        R"(#############################################################)",  //   3
        R"(#####..............###########..""""#########################)",  //   4
        R"(#####..............###########.#."""..""""""""""""""#########)",  //   5
        R"(#####..................#######........""""""""""""".....#####)",  //   6
        R"(#####..................###############"""""""""""....S..#####)",  //   7
        R"(#####""""##...####.....#......########"""""""""""N......#####)",  //   8
        R"(#####""""##...####.....#......########""""""""""".......D####)",  //   9
        R"(#####""""##..N####..#..#..S...########""""""""""".......#####)",  //  10
        R"(#####""""##...####.....#......########""""""""""".......#####)",  //  11
        R"(#####""""##...####.....#....""""""""##"""""""""S..."""..#####)",  //  12
        R"(#####""""##...####vvvvv#...""...".""##"""""""""...."""..#####)",  //  13
        R"(#####""................#....######""##.....""....##"""..#####)",  //  14
        R"(#####..................#....######""##vvvvvvvvvvv##"""..#####)",  //  15
        R"(#####....S.............#...."""N."""............."""""..#####)",  //  16
        R"(...........................""""..."".......N....."""""..#####)",  //  17
        R"(..........................."".....""............."""""..#####)",  //  18
        R"(..........................""""....."##..........""""""..#####)",  //  19
        R"(..........................""""".""""#########################)",  //  20
        R"(#############################################################)",  //  21
        R"(#############################################################)",  //  22
        R"(#############################################################)",  //  23
    });
    return grid;
}

//  The warp tile the Gate puts us on, and its open neighbours.
const std::vector<TilePos>& gate_exit(){
    static const std::vector<TilePos> tiles{{55, 9}, {54, 9}, {55, 8}, {55, 10}};
    return tiles;
}



}
}
}
}
