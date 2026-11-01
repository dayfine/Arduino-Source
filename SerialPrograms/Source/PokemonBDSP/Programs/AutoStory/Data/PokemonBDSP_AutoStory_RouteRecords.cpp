/*  BDSP AutoStory - Route records
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_RouteRecords.h"
#include "PokemonBDSP_AutoStory_JubilifeCity.h"
#include "PokemonBDSP_AutoStory_Route203.h"
#include "PokemonBDSP_AutoStory_Route204.h"
#include "PokemonBDSP_AutoStory_RavagedPath.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace RouteRecords{

using namespace Pokemon;

namespace{
const TileGrid* jubilife(){ return &JubilifeCity::grid(); }
const TileGrid* route203(){ return &Route203::grid(); }
const TileGrid* ravaged_path(){ return &RavagedPath::grid(); }
const TileGrid* route204_north(){ return &Route204::north_grid(); }
const TileGrid* none(){ return nullptr; }
}

//  The walks planned on a grid (00.04 and 00.05's walk_route goals, 01.03's
//  Mine legs, 01.06's Route 203 and Jubilife legs, 01.08's Route 204 South,
//  01.09's Ravaged Path, 01.10's waypoints) are data already, tested in
//  PokemonBDSP_AutoStory_Tests.cpp; these are the rest.
const std::vector<RouteRecord>& all(){
    static const std::vector<RouteRecord> records{
        {"01.01", "Jubilife Pokemon Center to Barry on Route 203", "Jubilife City", jubilife, false, std::nullopt, {
            {Direction::SOUTH, 0, LegEnd::NPC, "out of the Center's door; a girl below it stops us"},
            {Direction::WEST, 4, LegEnd::COUNT, "stick 720 ms: onto the main road"},
            {Direction::NORTH, 16, LegEnd::COUNT, "stick 3960 ms: up to the street east to Route 203"},
            {Direction::SOUTH, 2, LegEnd::COUNT, "stick 600 ms: back onto the street"},
            {Direction::EAST, 0, LegEnd::NPC, "along the street onto Route 203 until Barry stops us; rows retried 0, +1, +1, -3, -1 when a wall comes first"},
        }, "jubilife_to_barry(); stick holds from the console, 2026-10-29; the Jubilife grid's rows are 3/4 of a tile, so the counts are not traceable on it (task #21)"},
        {"01.02", "Barry to Oreburgh City through the Oreburgh Gate", "Route 203", route203, false, std::nullopt, {
            {Direction::EAST, 0, LegEnd::WALL, "along the row Barry stopped us on"},
            {Direction::NORTH, 0, LegEnd::WALL, "to the top wall, then a row south at a time until east goes on (up to ten)"},
            {Direction::EAST, 0, LegEnd::NPC, "to the Hiker"},
            {Direction::EAST, 0, LegEnd::TRANSITION, "into the Oreburgh Gate"},
            {Direction::EAST, 0, LegEnd::TRANSITION, "through the Gate into Oreburgh City"},
        }, "route203_to_oreburgh(); the west and middle of Route 203's grid are image-traced, the east stretch console-corrected (r227)"},
        {"01.04", "Oreburgh Pokemon Center doorstep to the Gym", "Oreburgh City", none, false, std::nullopt, {
            {Direction::WEST, 20, LegEnd::COUNT, "D-pad taps along the Center's row to below the Gym's door"},
            {Direction::NORTH, 1, LegEnd::DOOR, "into the Gym (the fade)"},
            {Direction::NORTH, 0, LegEnd::NPC, "up the Gym to its trainers and Roark"},
        }, "oreburgh_street_to_gym(), oreburgh_gym_battles(); 20 taps measured on the console (r204 to r215, 3 runs on the same tile)"},
        {"01.05", "Oreburgh Gym to the Gate and out onto Route 203", "Oreburgh City", none, false, std::nullopt, {
            {Direction::EAST, 20, LegEnd::COUNT, "D-pad taps from the Gym's doorstep back to the Center's door"},
            {Direction::WEST, 0, LegEnd::WALL, "to the stairs' cliff"},
            {Direction::NORTH, 0, LegEnd::WALL, "along the cliff"},
            {Direction::EAST, 2, LegEnd::COUNT, "to the stairs' column"},
            {Direction::NORTH, 0, LegEnd::WALL, "up the stairs"},
            {Direction::WEST, 0, LegEnd::WALL, "along the platform"},
            {Direction::SOUTH, 1, LegEnd::COUNT, "onto the Gate's row"},
            {Direction::WEST, 0, LegEnd::TRANSITION, "into the Oreburgh Gate (a row probed when a wall comes first)"},
            {Direction::WEST, 0, LegEnd::BANNER, "through the Gate until Route 203's banner"},
        }, "oreburgh_gym_to_center(), oreburgh_to_route203(); no Oreburgh City grid yet (decomp cells pending)"},
        {"01.07", "Jubilife's east edge to Rowan's scene", "Jubilife City", jubilife, false, std::nullopt, {
            {Direction::WEST, 14, LegEnd::COUNT, "stick 2 x 1120 ms along the long road to the crossroads with the north street"},
            {Direction::NORTH, 0, LegEnd::SCENE, "up the street until Rowan's scene starts (about 6 s); the green-haired man may stand in the way"},
        }, "jubilife_to_rowan(); by hand 2026-10-31 (18 presses), r230b; Jubilife grid not traceable (rows 3/4)"},
        {"01.08", "Jubilife's north street onto Route 204", "Jubilife City", jubilife, false, std::nullopt, {
            {Direction::NORTH, 0, LegEnd::BANNER, "up the street until Route 204's banner; the planned walk to the Ravaged Path door follows"},
        }, "jubilife_to_ravaged_path() step 1"},
        {"01.09", "Ravaged Path's east end out onto Route 204 North", "Ravaged Path", ravaged_path, true, RavagedPath::EAST_END, {
            {Direction::WEST, 1, LegEnd::COUNT, "onto the tile above the way out"},
            {Direction::SOUTH, 0, LegEnd::TRANSITION, "out of the cave"},
        }, "ravaged_path() exit; the planned legs before it are tested as plans"},
        {"01.10", "Route 204 North's path into Floaroma", "Route 204 North", route204_north, true, TilePos{11, 5}, {
            {Direction::NORTH, 0, LegEnd::BANNER, "from (11, 5) until Floaroma Town's banner; the waypoint line before it is tested as plans"},
        }, "route204_to_floaroma(); r248a"},
        {"01.11", "Floaroma's old couple", "Floaroma Town", none, false, std::nullopt, {
            {Direction::WEST, 8, LegEnd::COUNT, "D-pad taps along the flower field's bottom row"},
            {Direction::NORTH, 0, LegEnd::NPC, "until the old woman stops us (3 tiles on r251a)"},
            {Direction::WEST, 1, LegEnd::COUNT, "below the old man"},
            {Direction::NORTH, 0, LegEnd::NPC, "until the old man stops us (a turn)"},
        }, "floaroma_old_couple(); by hand 2026-10-31 (batches m1 to m9), r251a; Floaroma grid pending (decomp)"},
    };
    return records;
}


}
}
}
}
