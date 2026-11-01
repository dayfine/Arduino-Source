/*  BDSP AutoStory - Oreburgh Gym tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_OreburghGym.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace OreburghGym{

using namespace Pokemon;


//  Map source: the decomp (Diamond land_data 0228, 2026-10-31) with the
//      objects from its events file.
//  Console checks: none yet. 01.04 walks it as a record (north until a
//      trainer stops us), not as a plan.
//  Status: no checkpoint passes on this grid yet.
//
//  Decoded from Pokemon Diamond's collision table for this map (the
//  pret/pokediamond decompilation,
//  files/fielddata/land_data/land_data_release/narc_0228: the only cell of
//  matrix 113, at column 0, row 0; MAP_OREBURGH_GYM in
//  include/constants/maps.h), with the objects from its events file
//  (zone_event narc_0046, events_oreburgh_gym.json in Platinum); BDSP keeps
//  Diamond/Pearl's layout. Local (x, y) = world (x - 0, z - 0) in the Gym's
//  own matrix; 32 x 32 tiles, of which the floor is columns 1 to 10, rows 3
//  to 24. The tiers' edges and the pillars are '#'.
//  Door: (5, 24), walked into heading south to leave; coming in puts us on
//  (5, 23). Column 5 runs straight from there to Roark on row 3: the Gym is
//  walked north up the middle. Objects, from the events file:
//    Roark (5, 3), looks south;
//    Gym Guide (6, 23), looks south, beside the entrance;
//    Youngster (4, 18), looks east, sees 3: (5..7, 18);
//    Youngster (7, 11), looks west, sees 4: (3..6, 11);
//    the two wall signs, (3, 23) and (7, 23), read from row 23: 'S'.
//  Both trainers see the middle column, so a walk north from the entrance
//  is stopped twice before Roark (01.04's record).
const TileGrid& grid(){
    static const TileGrid grid({
    //     0         1         2         3
        R"(################################)",  //   0
        R"(################################)",  //   1
        R"(################################)",  //   2
        R"(#....N.....#####################)",  //   3
        R"(#..........#####################)",  //   4
        R"(#..........#####################)",  //   5
        R"(#..##.##...#####################)",  //   6
        R"(#..........#####################)",  //   7
        R"(#..##.##...#####################)",  //   8
        R"(#..........#####################)",  //   9
        R"(#..........#####################)",  //  10
        R"(###....N#.######################)",  //  11
        R"(#..........#####################)",  //  12
        R"(#####.##...#####################)",  //  13
        R"(#####.##...#####################)",  //  14
        R"(#..........#####################)",  //  15
        R"(#..........#####################)",  //  16
        R"(#..........#####################)",  //  17
        R"(#...N......#####################)",  //  18
        R"(##.#....########################)",  //  19
        R"(##.#....########################)",  //  20
        R"(#..........#####################)",  //  21
        R"(#..#...#...#####################)",  //  22
        R"(#..S..NS...#####################)",  //  23
        R"(#....D.....#####################)",  //  24
        R"(################################)",  //  25
        R"(################################)",  //  26
        R"(################################)",  //  27
        R"(################################)",  //  28
        R"(################################)",  //  29
        R"(################################)",  //  30
        R"(################################)",  //  31
    });
    return grid;
}

const TilePos ENTRANCE{5, 23};
const TilePos ROARK{5, 3};

std::vector<TrainerSight> trainer_sights(){
    return {
        {"Youngster (first tier)", {4, 18}, Direction::EAST, 3},
        {"Youngster (second tier)", {7, 11}, Direction::WEST, 4},
    };
}


}
}
}
}
