/*  BDSP AutoStory - Floaroma Town tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_FloaromaTown.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace FloaromaTown{

using namespace Pokemon;


//  Console: the old couple stands at (2, 25) and (3, 25) ('N'), placed from
//  the 2026-10-31 walk; they are not in the DP data (a Camper and a Lass are).
//  Map source: the decomp (Diamond land_data 0032 = Platinum map_data_032,
//      2026-10-31) with the objects from its events file.
//  Console checks: none yet.
//  Status: no checkpoint passes on this grid yet.
//
//  Decoded from Pokemon Diamond's collision table for this map (the
//  pret/pokediamond decompilation,
//  files/fielddata/land_data/land_data_release/narc_0032: one cell of the
//  Sinnoh matrix at column 5, row 20; map_data_032 in pret/pokeplatinum),
//  with the objects from its events file (zone_event narc_0404,
//  events_floaroma_town.json in Platinum); BDSP keeps Diamond/Pearl's layout.
//  Local (x, y) = world (x - 160, z - 640); 32 x 32 tiles. The cell above
//  (column 5, row 19, land_data 0033) carries the same map header but is cut
//  off from the town (its bottom rows are wall; the town's row 0 is a
//  dead-end strip under them), so it is left out.
//  The flower field is the west half, columns 0 to 17, rows 2 to 22;
//  Diamond's data puts a Camper at (6, 10) and a Lass wandering row 5 in it.
//  The old couple BDSP shows there are not in the Diamond/Pearl data: place
//  them as 'N' from the console. Exits: Route 204 south through (10..12,
//  30..31) (and the one-tile gap (8, 30..31), which joins a side path on the
//  route); Route 205 east at (31, 19..22). Warp tiles the player stands on
//  are written as '.': (20, 11), the Flower Shop's entrance, and (2, 1), (3,
//  1) into Floaroma Meadow, where the two Galactic grunts stand ('N' wins
//  there) until the Valley Windworks is cleared (flag 447). Objects, from the
//  events file:
//    door (16, 26) to the Pokemon Center; door (24, 26) to the south-east
//    house;
//    door (24, 17) to the Poke Mart; its signboard (29, 15);
//    door (11, 14) to the middle house;
//    Flower Shop entrance (20, 11), its signboard (17, 11) and the two berry
//    patches (18, 11) and (22, 11), as '#';
//    map signpost (13, 19);
//    Grunt M (2, 1), looks east, and Grunt M (3, 1), looks west, not
//    trainers, hidden by flag 447;
//    Camper (6, 10), looks around, not a trainer;
//    Lass (23, 11), looks west, not a trainer;
//    Pokemon Breeder F (19, 21), looks around, not a trainer;
//    a man with Cyrus's sprite (11, 26), looks around, not a trainer, hidden
//    by flag 449 (Diamond/Pearl only; whether he is there when the story
//    arrives is for the console);
//    Lass wanders west and east from (10, 5), columns 9 to 11, row 5: off the
//    grid;
//    School Kid M wanders around (16, 16), columns 15 to 17, rows 15 to 17:
//    off the grid.
//  No trainers. Platinum differs in one row and one person: its row 30 opens
//  columns 8 to 13 where Diamond opens 8 and 10 to 12 (Route 204 North's top
//  rows are '########.#...###' in both games, so Diamond's is the border BDSP
//  has), and it has a Beauty at (10, 19) looking south instead of the man at
//  (11, 26).
const TileGrid& grid(){
    static const TileGrid grid({
    //     0         1         2         3
        R"(######........................##)",  //   0
        R"(##NN##........................##)",  //   1
        R"(..............................##)",  //   2
        R"(..............................##)",  //   3
        R"(..............................##)",  //   4
        R"(..............................##)",  //   5
        R"(..............................##)",  //   6
        R"(..................#####.......##)",  //   7
        R"(..................#####.......##)",  //   8
        R"(..................#####.......##)",  //   9
        R"(......N...........#####.......##)",  //  10
        R"(..........####...S##.##N......##)",  //  11
        R"(..........####................##)",  //  12
        R"(..........####................##)",  //  13
        R"(..........#D##................##)",  //  14
        R"(.......................####..S##)",  //  15
        R"(.......................####...##)",  //  16
        R"(.......................#D##...##)",  //  17
        R"(..............................##)",  //  18
        R"(.............S..................)",  //  19
        R"(................................)",  //  20
        R"(...................N............)",  //  21
        R"(................................)",  //  22
        R"(..NN..........#####....####...##)",  //  23
        R"(..............#####....####...##)",  //  24
        R"(..............#####....####...##)",  //  25
        R"(...........N..##D##....#D##...##)",  //  26
        R"(..............................##)",  //  27
        R"(..............................##)",  //  28
        R"(..............................##)",  //  29
        R"(########.#...###################)",  //  30
        R"(########.#...###################)",  //  31
    });
    return grid;
}

const TilePos ROUTE_204_ARRIVAL{11, 29};
const TilePos OLD_WOMAN{3, 25};
const TilePos OLD_MAN{2, 25};
const TilePos POKEMON_CENTER_DOOR{16, 26};

}
}
}
}
