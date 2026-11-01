/*  BDSP AutoStory - Route 204 tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Route204.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route204{

using namespace Pokemon;


//  Map source: the decomp (map_data_031, 2026-10-31) with the objects from
//      its events file; before that the DP image, 3/4 rows.
//  Console checks: row 27 open under the cliff, the stairs, Taylor's sight at
//      column 17 (recorded human walk, 2026-10-31). Suspects: column 23 rows
//      12 to 19 blocked on the console (r242a); three open rows under the
//      cave, not four (r241a, r243a).
//  Status: 01.10 not passed yet; the route is the recorded human line
//      (floaroma_waypoints()).
//
//  Decoded from Pokemon Platinum's collision table for this map (the
//  pret/pokeplatinum decompilation, map_data_031.bin: one cell of the Sinnoh
//  matrix, column 5, row 21; local (x, y) = world (x - 160, z - 672)) with
//  the objects from its events file; BDSP keeps the layout. Water and the
//  items on the ground are '#'. Objects: the Ravaged Path's door, arriving
//  on (20, 26); the twins (15, 7) and (16, 7) looking south; Aroma Lady
//  Taylor (17, 20) looking south and east; Bug Catcher Brandon walking back
//  and forth, on row 12 on the console (r246a), drawn at (20, 12) by the DP
//  map, (22, 13) in Platinum's data; the arrow signpost
//  (17, 9); a Youngster who is not a trainer wanders rows 27 and 28 below
//  the cave, off the grid. The path north into Floaroma is columns 10 to 12
//  at the top.
//  The 2026-10-29 hand run went out of the cave, west to the stairs, up them
//  into the pocket the ledge (row 15) closes, back down, east and north
//  through the tall grass, and west below the flower beds past the twins.
//  Before 2026-10-31 this grid was traced from Bulbapedia's DP map image,
//  whose rows are 12 px to the columns' 16: every row was 3/4 of a tile.
const TileGrid& north_grid(){
    static const TileGrid grid({
    //     0         1         2         3
        R"(########.#...###################)",  //   0
        R"(########.#...###################)",  //   1
        R"(####.....#...########.##########)",  //   2
        R"(##########...#......#.##########)",  //   3
        R"(#####....#...#......#..........N)",  //   4
        R"(#####....#"""#......#...######..)",  //   5
        R"(#####....#"""###################)",  //   6
        R"(##########"""..NN.....##########)",  //   7
        R"(##........""""".......##########)",  //   8
        R"(##..######.""""..S....##########)",  //   9
        R"(###.#....#."""........##########)",  //  10
        R"(##..#....#."".........##########)",  //  11
        R"(##########..........N...########)",  //  12
        R"(##########..............########)",  //  13
        R"(##########...####"""""""########)",  //  14
        R"(##########vvv####"""""""########)",  //  15
        R"(##########...######"""""########)",  //  16
        R"(##########...######"""""########)",  //  17
        R"(##########...######"""""########)",  //  18
        R"(##########...######"."""########)",  //  19
        R"(##########.......N....""########)",  //  20
        R"(##########............##########)",  //  21
        R"(##########............##########)",  //  22
        R"(##########...##.....#.##########)",  //  23
        R"(##########...##.......##########)",  //  24
        R"(##########...###################)",  //  25
        R"(##########...#######.###########)",  //  26
        R"(##########............##########)",  //  27
        R"(##########............##########)",  //  28
        R"(##########............##########)",  //  29
        R"(##########............##########)",  //  30
        R"(##########...###################)",  //  31
    });
    return grid;
}

const TilePos CAVE_EXIT{20, 26};

bool in_floaroma_path(const TilePos& pos){
    return pos.x >= 10 && pos.x <= 12 && pos.y <= 5;
}

std::vector<TrainerSight> north_trainer_sights(){
    return {
        {"Twins (left)",  {15, 7}, Direction::SOUTH, 3},
        {"Twins (right)", {16, 7}, Direction::SOUTH, 4},
        {"Aroma Lady Taylor", {17, 20}, Direction::SOUTH, 4},
        {"Aroma Lady Taylor", {17, 20}, Direction::EAST, 4},
        //  Bug Catcher Brandon paces east and west below the flower beds
        //  and stopped us on row 12 (r246a: "Say, what's your opinion on bug
        //  Pokemon?"); the DP map draws him at (20, 12), Platinum's data
        //  has him at (22, 13). The tile is a guess until a shot fixes it.
        {"Bug Catcher Brandon", {20, 12}, Direction::WEST, 4},
        {"Bug Catcher Brandon", {20, 12}, Direction::EAST, 4},
    };
}

std::vector<TilePos> floaroma_waypoints(){
    //  The line a human walked on 2026-10-31 (recorded; drawn over the DP
    //  map image): a tile south out of the cave's mouth; west along the row
    //  under the cliff, where the Youngster wanders (he stood a row below
    //  as we passed; the walker waits for him when he is in the way); north
    //  up the middle of the stairs; east along the corridor, where Aroma
    //  Lady Taylor sees us at column 17 and walks over for her battle; north
    //  up column 19 through the tall grass; west below the flower beds;
    //  north between the beds. The DP image shows a trainer sprite at
    //  (20, 12), by the corner where the line turns west: not seen yet on
    //  the console. The ledge on row 15 closes the stairs' column going
    //  north, so the line goes round through the grass.
    return {{20, 27}, {11, 27}, {11, 22}, {19, 22}, {19, 12}, {12, 12}, {12, 5}};
}


}
}
}
}
