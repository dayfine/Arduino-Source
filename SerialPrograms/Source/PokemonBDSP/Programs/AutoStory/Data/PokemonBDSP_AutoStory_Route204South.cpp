/*  BDSP AutoStory - Route 204 South tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Route204South.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Route204South{

using namespace Pokemon;


//  Map source: the decomp (map_data_021, 2026-10-31) with the objects from
//      its events file.
//  Console checks: column 14 stops at the signpost (14,17), column 13 is
//      grass to the cliff, Sarah fights on row 21 (r233a, r234a, probes).
//  Status: 01.08 passes once (r235a).
//
//  Decoded from Pokemon Platinum's collision table for this map (the
//  pret/pokeplatinum decompilation, res/field/maps/data/map_data_021.bin:
//  32 x 32 tiles, one cell of the Sinnoh matrix at column 5, row 22), with
//  the objects from its events file (events_route_204_south.json); BDSP
//  keeps Diamond/Pearl's layout. Local (x, y) = world (x - 160, z - 704).
//  Water is '#'. Objects, from the events file:
//    door to the Ravaged Path (11, 1); its signboard (10, 2);
//    the "204" arrow signpost (14, 17), which stops a walk north up the
//    street's column 14 (r233a, r234a);
//    Lass Samantha (11, 8), looks south; Lass Sarah (12, 21), looks north
//    and east by turns; Youngster Tyler (7, 18), looks west and east;
//    items on the ground (17, 18), (29, 12), (2, 28), as '#';
//    a Youngster who is not a trainer wanders west and east along row 2 in
//    front of the door: off the grid, the walker waits for him.
//  Checked on the console 2026-10-31 (r233a, r234a, probes): north up
//  column 14 from Jubilife stops at the signpost (14, 17); column 13 is
//  tall grass all the way up and stops at the cliff (13, 2), the door two
//  columns west; Sarah walked up and fought us once on row 21.
//  Row 31, columns 12 to 15, joins the top of Jubilife's north street:
//  walking south there leaves the route.
const TileGrid& grid(){
    static const TileGrid grid({
        R"(################################)",  //   0
        R"(###########D###.################)",  //   1
        R"(########..S.....################)",  //   2
        R"(########........################)",  //   3
        R"(########........################)",  //   4
        R"(##########..."##################)",  //   5
        R"(##########..."##################)",  //   6
        R"(##########..""##################)",  //   7
        R"(##########.N""##################)",  //   8
        R"(####""""""""""##################)",  //   9
        R"(####""""""""""##################)",  //  10
        R"(####""""""""""##################)",  //  11
        R"(####""""""""""############...###)",  //  12
        R"(####""""""""""############....##)",  //  13
        R"(####""""""""""#..###############)",  //  14
        R"(##""""""""""""#..###############)",  //  15
        R"(##""""""""""""#..###############)",  //  16
        R"(##""""""""""""S.################)",  //  17
        R"(##"""""N........################)",  //  18
        R"(##""""".........#.##############)",  //  19
        R"(##""""".........#.##############)",  //  20
        R"(##.#########N...#.##############)",  //  21
        R"(############....#......"########)",  //  22
        R"(############....######""########)",  //  23
        R"(############....######""########)",  //  24
        R"(############....######""########)",  //  25
        R"(############....######""########)",  //  26
        R"(############....######""########)",  //  27
        R"(###.######.#....#.."""""########)",  //  28
        R"(##..######.#....#...""""########)",  //  29
        R"(##########.#....#.......########)",  //  30
        R"(############....######..########)",  //  31
    });
    return grid;
}

std::vector<TilePos> jubilife_entry(){
    //  The walk north looks between 400 ms holds, so it may stop up to two
    //  tiles past the border.
    return {{14, 31}, {14, 30}, {14, 29}};
}

const TilePos BELOW_DOOR{11, 2};

std::vector<TrainerSight> trainer_sights(){
    //  Sarah turns between north and east; her east line crosses the
    //  street's columns, so a walk north meets it or not by luck (r233a
    //  passed, r234a fought). The planner cannot avoid it: she is listed
    //  so the route knows whom it may meet, with the range that fits.
    return {
        {"Lass Samantha", {11, 8},  Direction::SOUTH, 4},
        {"Lass Sarah",    {12, 21}, Direction::NORTH, 4},
        {"Lass Sarah",    {12, 21}, Direction::EAST,  4},
        {"Youngster Tyler", {7, 18}, Direction::EAST, 4},
        {"Youngster Tyler", {7, 18}, Direction::WEST, 4},
    };
}

std::vector<TilePos> tiles_to_avoid(){
    std::vector<TilePos> tiles;
    for (const TrainerSight& trainer : trainer_sights()){
        if (trainer.name == "Lass Sarah" && trainer.facing == Direction::EAST){
            continue;
        }
        for (const TilePos& pos : sight_tiles(grid(), trainer)){
            tiles.push_back(pos);
        }
    }
    return tiles;
}


}
}
}
}
