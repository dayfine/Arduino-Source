/*  BDSP AutoStory - Oreburgh City tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_MapGrids.h"
#include "PokemonBDSP_AutoStory_OreburghCity.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace OreburghCity{

using namespace Pokemon;


//  Map source: the decomp (Diamond land_data 0022, 0023, 0175, 0024 =
//      Platinum map_data_022/023/175/024, 2026-10-31) with the objects from
//      its events file.
//  Console checks: none yet.
//  Status: no checkpoint passes on this grid yet.
//
//  Decoded from Pokemon Diamond's collision table for this map (the
//  pret/pokediamond decompilation,
//  files/fielddata/land_data/land_data_release/narc_0022, 0023, 0175 and
//  0024: four cells of the Sinnoh matrix, columns 8 and 9, rows 23 and 24;
//  the same cells are map_data_022, 023, 175 and 024 in pret/pokeplatinum),
//  with the objects from its events file (zone_event narc_0044,
//  events_oreburgh_city.json in Platinum); BDSP keeps Diamond/Pearl's layout.
//  Local (x, y) = world (x - 256, z - 736); 64 x 64 tiles. The town sits on
//  the upper rows (10 to 26); row 27 is the cliff, and the stairs down it are
//  the road at columns 44 to 48 (rows 27 to 43), which opens into the mine
//  yard at row 44. The two one-tile stubs south of the gym, (24, 27..29) and
//  (31, 27..29), and the pocket at (35..42, 28..29) are walkable in the data
//  but closed off: dead ends, not stairs.
//  Exits: Oreburgh Gate west through the cave mouth (1, 13), entered from the
//  warp tile (2, 13); Route 207 north up columns 41 to 46 (rows 0 to 9); the
//  Mine south over the warp tiles (44..48, 59), walked onto heading south.
//  Warp tiles the player stands on are written as '.': (2, 13), (44..48, 59),
//  and (49, 11), the Mining Museum's entrance. Objects, from the events file:
//    door (26, 20) to the Gym; its signpost (22, 20);
//    door (47, 20) to the Pokemon Center;
//    door (29, 10) to the Poke Mart; doors (15, 10) and (22, 10) to the
//    north-west and north houses; door (17, 20) to the west house; door (37,
//    16) to the middle house; door (56, 28) to the east house; door (51, 36)
//    to the south house;
//    map signposts (4, 11) and (44, 1); signboards (45, 12) and (45, 56);
//    steam vents (9, 26), (11, 26), (13, 26), (60, 52), (60, 53), objects
//    that block, as '#';
//    Youngster (10, 16), looks north, not a trainer;
//    Worker (11, 25), looks south and west, not a trainer;
//    Hiker wanders around (47, 26), columns 46 to 48, rows 25 to 27: off the
//    grid;
//    School Kid F wanders around (32, 18), column 32, rows 16 to 20: off the
//    grid;
//    Guitarist wanders around (23, 13), columns 22 to 24, rows 10 to 16: off
//    the grid;
//    Camper walks back and forth from (36, 19), columns 33 to 39, row 19: off
//    the grid;
//    Battle Girl wanders around (36, 23), column 36, rows 21 to 25: off the
//    grid;
//    Pokefan F wanders around (45, 38), column 45, rows 37 to 39: off the
//    grid;
//    Worker wanders around (37, 39), columns 36 to 38, rows 38 to 40: off the
//    grid;
//    Worker wanders around (50, 41), column 50, rows 38 to 44: off the grid;
//    Worker wanders around (56, 57), columns 55 to 57, rows 54 to 60: off the
//    grid;
//    Machop wanders around (55, 56), columns 53 to 57, rows 54 to 58: off the
//    grid;
//    Barry (26, 21), in front of the Gym door, "wander" with no range (hidden
//    by flag 410 once the story moves on): off the grid, listed so the route
//    knows he may stand there.
//  None of the people here is a trainer. Platinum (pret/pokeplatinum) decodes
//  the same terrain tile for tile; it adds items on the ground at (35, 37)
//  and (39, 38), a Battle Girl at (51, 21), a Machop at (56, 46) and a
//  wandering Machop around (35, 45), none of which Diamond has.
const TileGrid& grid(){
    return story_grid("OreburghCity");
}

const TilePos POKEMON_CENTER_DOOR{47, 20};
const TilePos POKEMON_CENTER_DOORSTEP{46, 21};
const TilePos GYM_DOOR{26, 20};
const TilePos GYM_DOORSTEP{26, 21};
const TilePos GATE_MOUTH{1, 13};

}
}
}
}
