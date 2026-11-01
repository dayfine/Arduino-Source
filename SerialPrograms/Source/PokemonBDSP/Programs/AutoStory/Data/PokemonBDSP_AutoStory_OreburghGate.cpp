/*  BDSP AutoStory - Oreburgh Gate 1F tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_MapGrids.h"
#include "PokemonBDSP_AutoStory_OreburghGate.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace OreburghGate{

using namespace Pokemon;


//  Map source: the decomp (Diamond land_data 0418 = Platinum map_data_441,
//      2026-10-31) with the objects from its events file.
//  Console checks: none yet.
//  Status: no checkpoint passes on this grid yet.
//
//  Decoded from Pokemon Diamond's collision table for this map (the
//  pret/pokediamond decompilation,
//  files/fielddata/land_data/land_data_release/narc_0418: one cell, the only
//  one of matrix 003, at column 0, row 0; map_data_441 in pret/pokeplatinum),
//  with the objects from its events file (zone_event narc_0250,
//  events_oreburgh_gate_1f.json in Platinum); BDSP keeps Diamond/Pearl's
//  layout. Local (x, y) = world (x - 0, z - 0) in the cave's own matrix; 32 x
//  32 tiles. The cave floor is '.'; the rocks Rock Smash breaks are '#'
//  (eight of them, each hidden once smashed).
//  Exits: the warp tiles the player stands on are written as '.': (4, 22)
//  leaves west to Route 203 and (27, 22) east to Oreburgh City; the cave
//  mouths behind them, (3, 22) and (28, 22), are 'D'. (20, 5) is the stairs
//  down to B1F (a warp tile too). Objects, from the events file:
//    Picnicker Diana (24, 21), looks south, sees 3;
//    Camper Curtis (17, 25), looks north, sees 3;
//    Hiker (7, 20), looks south, not a trainer (the Rock Smash HM);
//    rocks (19, 6), (14, 4), (12, 10), (11, 11), (11, 25), (13, 26), (15,
//    26), (25, 24), as '#'.
//  The two trainers face the row-22 corridor from either side: Diana sees
//  (24, 22..24) and Curtis (17, 22..24). Platinum decodes this cave
//  identically, terrain and objects.
const TileGrid& grid(){
    return story_grid("OreburghGate");
}

}
}
}
}
