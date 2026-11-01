/*  BDSP AutoStory - Route 202 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Route202.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route202{

using namespace Pokemon;


//  Map source: the decomp (pret/pokediamond land_data narc_0014, with the
//      margins from narc_0006, 0007 and 0173; 2026-10-31) and the objects
//      from its events file; Platinum's map_data_014 has the same terrain.
//  Console checks: the player under the south ledge at (22,24) beside the
//      trees at (21,24), the sign a row below at (24,25), the save spot
//      (30,24) against the east trees, Natalie's challenge at (22,22), east
//      along the row above the ledge stops at column 30 and column 30 runs
//      north to row 4 (r75), the catch spot's walls (2026-10-29/30).
//  Status: no checkpoint has run on this grid yet. 00.04 passed on the
//      image-traced grid it replaces (26 rows for 32) and must be re-run;
//      its legs south of the catch spot are three rows longer here.
//
//  Decoded from Pokemon Diamond's collision table: cell (25,5) of the Sinnoh
//  matrix, with the objects from its events file. Local (x, y) = world
//  (x - 159, z - 800); 36 x 32 tiles, the whole cell, so the grid's edges
//  are where the game changes its location banner. The columns are the ones
//  the image-traced grid had (its ledges, tree pillars and signs matched the
//  decomp exactly); the rows are the game's: the image gave 26 rows for 32.
//  Sandgem Town's grid starts on the row below row 31, at column 1.
//
//  Exits: north, the gap (11..18, 0) to Jubilife City (EXIT_JUBILIFE in its
//  middle); south, (21..30, 31) into Sandgem, where the road narrows to
//  25..30 for two rows. Ledges: row 10 (9..13 and
//  22..27) and row 23 (9..13 and 22..30), jumped southward only. The east
//  wall is column 33 from row 10 to 20, column 32 on row 21 and column 31 on
//  row 22, the row right above the south ledge: east along that row stops at
//  column 30 (r75), and column 30 is open north from there to row 4 under
//  Logan's row.
//  Objects, from the events file: three trainers who stand still, Youngster
//  Tristan (7,13), Lass Natalie (22,18) and Youngster Logan (26,4), all
//  facing south, with the sight ranges the events give; a Potion ball (3,6),
//  a wall until picked up; the signs (17,4) to Jubilife, (15,16) Trainer Tips
//  and (24,25) to Sandgem. A Youngster who is not a trainer wanders along row
//  8, columns 9..11, and is left off the grid (the image-traced grid had him
//  as a fourth trainer facing east). Dawn's tile (15,27) is hidden by a
//  story flag.
const TileGrid& grid(){
    //     0         1         2         3
    //     012345678901234567890123456789012345
    static const TileGrid grid({
        R"(###########........#################)",  //   0
        R"(###########........#################)",  //   1
        R"(###########........#################)",  //   2
        R"(###########........#################)",  //   3
        R"(###""""""........S..""""".N....#####)",  //   4
        R"(###""""""..........."""""......#####)",  //   5
        R"(####"""""..........."""""......#####)",  //   6
        R"(###""""""..........."""""......#####)",  //   7
        R"(###""""""..........."""""......#####)",  //   8
        R"(###"""""".....########.........#####)",  //   9
        R"(#########vvvvv########vvvvvv...""###)",  //  10
        R"(#########.....########"""""""""""###)",  //  11
        R"(#######.......########"""""""""""###)",  //  12
        R"(#######N..........####"""""""""""###)",  //  13
        R"(###...............####"""""""""""###)",  //  14
        R"(###...............####"""""""""""###)",  //  15
        R"(###............S..####"""""""""""###)",  //  16
        R"(###...............####"""""""""""###)",  //  17
        R"(###...............####N."""""""""###)",  //  18
        R"(###""""##..............."""""""""###)",  //  19
        R"(###""""##................."""""""###)",  //  20
        R"(###""""##....................""""###)",  //  21
        R"(###""""##......................#####)",  //  22
        R"(###""""##vvvvv########vvvvvvvvv#####)",  //  23
        R"(###""""##.....########.........#####)",  //  24
        R"(###""""""......""""""...S......#####)",  //  25
        R"(###""""""......""""""..........#####)",  //  26
        R"(###""""""......""""""..........#####)",  //  27
        R"(###""""""......""""""..........#####)",  //  28
        R"(###""""""......""""""..........#####)",  //  29
        R"(#####################..........#####)",  //  30
        R"(#####################..........#####)",  //  31
    });
    return grid;
}


//  Where 00.03's save after the catch stands: the middle row of the
//  tall-grass patch at (15..20, 25..29), world (178,827). On the console the
//  sign at (24,25) is 5 columns right and 2 rows up, walking east stops at
//  the trees after 11 tiles and north at the trees after 2; the patch goes
//  on two rows below.
const TilePos CATCH_SPOT{19, 27};
//  Where 00.04 saves once it has Potions, before Route 202's trainers (the
//  console's saves from r113a..r117a stand here too): under the south ledge,
//  against the east trees, world (189,824).
const TilePos SAVE_SPOT{30, 24};

//  From the events file: each trainer's tile, facing and sight range.
//  Natalie challenged us at (22,22), walking east along the row above the
//  ledge (r75), the fourth tile of her sight.
const TilePos TRAINER_TRISTAN{7, 13};
const TilePos TRAINER_NATALIE{22, 18};
const TilePos TRAINER_LOGAN{26, 4};

const std::vector<TrainerSight>& trainer_sights(){
    static const std::vector<TrainerSight> sights{
        {"tristan", TRAINER_TRISTAN, Direction::SOUTH, 5},
        {"natalie", TRAINER_NATALIE, Direction::SOUTH, 4},
        {"logan", TRAINER_LOGAN, Direction::SOUTH, 5},
    };
    return sights;
}

//  The middle of the 8-tile gap at (11..18, 0), world (174,800), below the
//  3-wide neck of Jubilife's south exit (173..175).
const TilePos EXIT_JUBILIFE{15, 0};



}
}
}
}
