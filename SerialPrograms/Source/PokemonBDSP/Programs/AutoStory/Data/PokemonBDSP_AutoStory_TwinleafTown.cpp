/*  BDSP AutoStory - Twinleaf Town tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_MapGrids.h"
#include "PokemonBDSP_AutoStory_TwinleafTown.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace TwinleafTown{

using namespace Pokemon;


//  Map source: the decomp (Diamond land_data 0000 = Platinum map_data_000,
//      2026-10-31) with the objects from its events file.
//  Console checks: none yet.
//  Status: no checkpoint passes on this grid yet.
//
//  Decoded from Pokemon Diamond's collision table for this map (the
//  pret/pokediamond decompilation,
//  files/fielddata/land_data/land_data_release/narc_0000: one cell of the
//  Sinnoh matrix at column 3, row 27; map_data_000 in pret/pokeplatinum),
//  with the objects from its events file (zone_event narc_0389,
//  events_twinleaf_town.json in Platinum); BDSP keeps Diamond/Pearl's layout.
//  Local (x, y) = world (x - 96, z - 864); 32 x 32 tiles. The pond at the
//  bottom, (12..19, 27..31), is water, '#'. The pockets (7..10, 1..2) and
//  (21..24, 1..2) are walkable in the data but closed in.
//  Exit: Route 201 north through columns 12 to 19 at row 0 (the route's
//  bottom rows open the same eight columns). The player's house is the
//  south-east one, door (20, 21), and Barry's the north-west one, door (9,
//  11): behind (20, 21) the interior has Mom and the TV, behind (9, 11) only
//  Barry's mother, and Platinum's scripts name the mailbox at (18, 21) the
//  player's. pret/pokediamond's map constants call them the other way round
//  (MAP_TWINLEAF_PLAYER_HOUSE_1F is the house behind (9, 11)); the interiors
//  say otherwise. Objects, from the events file:
//    door (20, 21), the player's house; its mailbox (18, 21), read like a
//    sign, 'S';
//    door (9, 11), Barry's house; its mailbox (13, 11), 'S'; Barry himself
//    stands on the door tile at the start (looks south, hidden by flag 402):
//    the door keeps its 'D';
//    door (20, 11) to the north-east house; door (10, 21) to the south-west
//    house;
//    map signpost (16, 14);
//    Guitarist (12, 5), looks north, west and east, not a trainer;
//    Collector (12, 25), looks around, not a trainer;
//    Pokemon Breeder F wanders around (23, 14), columns 22 to 24, row 14: off
//    the grid.
//  No trainers. Platinum decodes the same terrain and objects; its one extra,
//  an arrow signpost at world (109, 856), stands in Route 201's cell.
const TileGrid& grid(){
    return story_grid("TwinleafTown");
}

}
}
}
}
