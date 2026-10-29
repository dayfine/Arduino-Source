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


//  Traced by hand from Bulbapedia's Sinnoh_Route_204_DP.png (512x848, 16 px
//  tiles, not squashed), rows 0 to 27 of 53: the north half, above the
//  cliff the Ravaged Path runs under. Checked against the hand run of
//  2026-10-29: out of the cave onto the sand below the cliff, west to the
//  stairs, up them into a pocket closed at the top by a ledge (row 12, 'v',
//  only crossed southward), back down to the sand, east and north through
//  the tall grass to the flower beds, the twins below the right-hand bed,
//  and north between the beds' fences into Floaroma.
//
//  Not yet checked tile by tile on the console: the fence and tree edges
//  fall on tile boundaries in the image, so a column may be off by one.
//  '?' is the way down the cliff to the south half (not traced).
const TileGrid& north_grid(){
    static const TileGrid grid({
    //     0         1         2         3
        R"(#########....###################)",  //   0
        R"(#########....###################)",  //   1
        R"(#########....#######.###########)",  //   2
        R"(#########....#######.###########)",  //   3
        R"(#########....#######.###########)",  //   4
        R"(#########....#######.###########)",  //   5
        R"(#########.....NN.....###########)",  //   6
        R"(#...............S....###########)",  //   7
        R"(#...#####............###########)",  //   8
        R"(##########...........###########)",  //   9
        R"(##########..#####...N..#########)",  //  10
        R"(##########..######.....#########)",  //  11
        R"(##########vv#######""""#########)",  //  12
        R"(##########..#######""""#########)",  //  13
        R"(##########..#######""""#########)",  //  14
        R"(##########..#######""""#########)",  //  15
        R"(##########.......N.""""#########)",  //  16
        R"(##########...........###########)",  //  17
        R"(##########..###......###########)",  //  18
        R"(##########..###....#.###########)",  //  19
        R"(##########..####################)",  //  20
        R"(##########..####################)",  //  21
        R"(##########..########D###########)",  //  22
        R"(##########....N......###########)",  //  23
        R"(####..................##########)",  //  24
        R"(####..................##########)",  //  25
        R"(##########???###################)",  //  26
        R"(################################)",  //  27
    });
    return grid;
}

const TilePos CAVE_EXIT{20, 23};

bool in_floaroma_path(const TilePos& pos){
    return pos.x >= 9 && pos.x <= 12 && pos.y >= 6 && pos.y <= 8;
}

std::vector<TrainerSight> north_trainer_sights(){
    return {
        {"Twins (left)",  {14, 6}, GridDirection::SOUTH, 3},
        {"Twins (right)", {15, 6}, GridDirection::SOUTH, 3},
    };
}


}
}
}
}
