/*  BDSP AutoStory - Jubilife City tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <vector>
#include "PokemonBDSP_AutoStory_JubilifeCity.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace JubilifeCity{

using namespace Pokemon;


//  Map source: the decomp (pret/pokediamond land_data narc_0018, 0016, 0017
//      and 0015, 2026-10-31) with the objects from its events file. Platinum
//      differs only around the Global Terminal (columns 14..26, rows 30..45).
//  Console checks (2026-10-30/31): the lamps (43,53) and (49,53), north from
//      (52,53) to the Pokemon Center's door, the School's door from the
//      Center's doorstep, the east street to Route 203 (01.06, r230a), the
//      north street (01.07), and the four 00.05 people, placed from the
//      console on the tiles the events file gives them.
//  Status: no checkpoint has run on this grid yet. 00.05, 01.01, 01.06 and
//      01.07 passed on the colour-classified image grid it replaces (48 rows
//      for 64) and must be re-run; re-check the Mart clown's tile (he wanders
//      182..184 on row 33 in Diamond) and the hand walk from the steps' top
//      east to the house and north to the Center (the decomp has the plaza
//      only 171..177 wide on rows 58..59).
//
//  Decoded from Pokemon Diamond's collision table: cells (23..24, 4..5) of
//  the Sinnoh matrix, with the objects from its events file. Local (x, y) =
//  world (x - 128, z - 736); 64 x 64 tiles. The console's measurements on
//  the image grid were a column east of its doors; on this grid the lamps,
//  the doors and the four people all land where the console put them.
//
//  Exits: south, the 3-wide neck (45..47, 61) down to Route 202; east, the
//  street (rows 21..24) to Route 203; north, (42..55, 0..1) to Route 204;
//  west, the Route 218 gate on rows 22..23 (its warp tiles are 'D'). Doors
//  are 'D' tiles walked into from below; the Trainers' School's doorway
//  (40,40) and the Global Terminal's (21,42) are warp tiles stepped onto and
//  marked 'D' as well, so a plan never steps on them. The main road runs
//  from the steps (46..47, 60..62) north between the lamps (43,53), (49,53)
//  and on up to the north exit; the doors' row 40 is open between the School
//  and the Center (43..49), and the plaza below it is rows 41..52.
//  People who stand still, from the events file: the Poketch president
//  (46,35) and the clown west of the TV station (36,16), the 00.05 story
//  people (hidden by flags before and after); an Ace Trainer (36,25) on the
//  north road's west side, a Cashier (26,42), an Expert (44,44), two kids
//  (52,46), (54,46) on the plaza and a School Kid (17,55): none of these
//  five has been seen on the console yet. The clown by the Mart (55,33) and
//  the one by the Poketch Company (15,18) wander three tiles in Diamond but
//  stood still on the console (00.05, three runs): on the grid at their
//  spawn tiles. Wanderers left off: an Ace Trainer along row 20 (21..23),
//  one along row 21 (45..47), a Twin around (33,18), a School Kid on column
//  51 (rows 33..35), a Pokefan around (41,30). Rowan and the grunts at the
//  north exit, the Collectors and Dawn are hidden by story flags.
namespace{
//  Rows 0..31: the matrix's cell row 4 (world z 736..767), the north half.
std::vector<std::string> north_half(){
    //     0         1         2         3         4         5         6
    //     0123456789012345678901234567890123456789012345678901234567890123
    return {
        R"(##########################################..............########)",  //   0
        R"(##########################################..............########)",  //   1
        R"(############################################.....###############)",  //   2
        R"(############################################......##############)",  //   3
        R"(############################################......##############)",  //   4
        R"(############################################......##############)",  //   5
        R"(############################################......##############)",  //   6
        R"(#############################################....###############)",  //   7
        R"(############################....#########.......S.##############)",  //   8
        R"(############################....#########.........##############)",  //   9
        R"(#########..#########..######....#########.........##############)",  //  10
        R"(#########..#########..######....#########.........##############)",  //  11
        R"(#########..#########..######....#########.........##############)",  //  12
        R"(#########..#########..######....#########.........##############)",  //  13
        R"(#########..#########..######....#########.........##############)",  //  14
        R"(#########..##D###D##..######....####D####.........##############)",  //  15
        R"(#########.............######........N......#.....#######....####)",  //  16
        R"(#########.............######......................######....####)",  //  17
        R"(..#######......N......######......................######....####)",  //  18
        R"(..###########S......########...........S.........S##D###....####)",  //  19
        R"(..#.S.#.......................#..........................#.S####)",  //  20
        R"(................................................................)",  //  21
        R"(D...............................................................)",  //  22
        R"(D...............................................................)",  //  23
        R"(................................................................)",  //  24
        R"(..#...#.......................#.....N....................#..####)",  //  25
        R"(..############.............####......####...................####)",  //  26
        R"(..############.............####......####................#######)",  //  27
        R"(##############.################...##.####..#.....######..#######)",  //  28
        R"(##############.################...##.####.........#####..#######)",  //  29
        R"(##############.################...................#D############)",  //  30
        R"(################################.........................#######)",  //  31
    };
}
//  Rows 32..63: cell row 5 (world z 768..799), the south half.
std::vector<std::string> south_half(){
    //     0         1         2         3         4         5         6
    //     0123456789012345678901234567890123456789012345678901234567890123
    return {
        R"(################################.........................##D####)",  //  32
        R"(################################.......................N.#######)",  //  33
        R"(################################.........................#######)",  //  34
        R"(###############............#####..............N..........#######)",  //  35
        R"(###############............#####......#####..............#######)",  //  36
        R"(###############..#########.#####.##########.......######.#######)",  //  37
        R"(###############..#########.#####.##########.......######.#######)",  //  38
        R"(###############..#########.#####.###########.....#######.#######)",  //  39
        R"(##################################..S.##D##.......##D###########)",  //  40
        R"(###############..#########.#####.........................#######)",  //  41
        R"(##############...####D####N.###..........................#######)",  //  42
        R"(##############...........................................#######)",  //  43
        R"(##############.....S........................N............#######)",  //  44
        R"(##############...........................................#######)",  //  45
        R"(##############.............#########................N.N..#######)",  //  46
        R"(###############............#########.#####...............#######)",  //  47
        R"(###############............#########.#####...............#######)",  //  48
        R"(###############.####..####.###############...............#######)",  //  49
        R"(###############.####..####.###############...............#######)",  //  50
        R"(###############.####..####.###############...........###########)",  //  51
        R"(############D##.####..####.############D##...........###########)",  //  52
        R"(##########.................#########.......#.....#...########.##)",  //  53
        R"(##########.................#########.................########.##)",  //  54
        R"(##########.......N....#..#.#########......S..........########.##)",  //  55
        R"(##########............#..#.#########.................########.##)",  //  56
        R"(##########.................#########.................########.##)",  //  57
        R"(###########################################.......##############)",  //  58
        R"(###########################################.......##############)",  //  59
        R"(########################################.##.......##############)",  //  60
        R"(########################################.####...################)",  //  61
        R"(########################################..........##############)",  //  62
        R"(########################################..........##############)",  //  63
    };
}
}

const TileGrid& grid(){
    static const TileGrid grid = []{
        std::vector<std::string> rows = north_half();
        const std::vector<std::string> south = south_half();
        rows.insert(rows.end(), south.begin(), south.end());
        return TileGrid(rows);
    }();
    return grid;
}

//  Checked on the console (2026-10-30, CFR "walk"): 00.05's save stands
//  below the steps, against the wall just east of them (the banner reads
//  Jubilife City). West along that wall does not stop at the steps (it runs
//  to the trees); a tile west, then north, climbs them. North up the main
//  road from the steps passes the lamps (43,53), (49,53) and does not stop
//  before the north exit. From the lamps' row, west stops at the house at
//  (37..42, 47..52) and east at the house at (53..57, 49..56); north from
//  (52,53) stops in front of the Pokemon Center. The steps are columns
//  45..47 of row 61; the save stands at (48,62) against (48,61), and the
//  tile west then north puts us on column 47, a few rows up. On this grid
//  east from rows 58..60 stops at column 49 and north from there at the lamp
//  (49,53), not the Center: the climb must end on row 57 or above, or BDSP's
//  plaza is wider than Diamond's there (re-check on the console).
const std::vector<TilePos>& up_the_steps(){
    static const std::vector<TilePos> tiles{{46, 58}, {47, 58}, {46, 59}, {47, 59}, {46, 60}, {47, 60}};
    return tiles;
}

//  The School's doorway is a warp tile stepped onto from (40,41); the
//  Center's door is walked into from (52,41). On the console (2026-10-30)
//  eleven one-tile holds west from the Center's doorstep put us under the
//  School's door: the doorstep was the tile east of the door's column, as
//  story_door_front() allows.
const TilePos TRAINERS_SCHOOL_DOOR{40, 40};
const TilePos POKEMON_CENTER_DOOR{52, 40};
const TilePos TV_STATION_DOOR{36, 15};

//  On the main road, five rows north of the School's and the Center's
//  doors' row and six columns west of the Center's door. Talked to from the
//  east, facing west (also works from the south).
const TilePos POKETCH_PRESIDENT{46, 35};
//  Below the Poke Mart's planters, where the road meets the plaza: his spawn
//  tile; he wanders a tile either side in Diamond, and the console had him
//  a tile east of it.
const TilePos CLOWN_MART{55, 33};
//  West of the TV station door, right below it.
const TilePos CLOWN_TV_STATION{36, 16};
//  East of the Poketch Company's sign, a row below it. Talked to from the
//  south on the console; from the east a walk west along row 18 stops right
//  beside him.
const TilePos CLOWN_POKETCH_COMPANY{15, 18};



}
}
}
}
