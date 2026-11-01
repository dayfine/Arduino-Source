/*  BDSP AutoStory - Route records
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_RouteRecords.h"
#include "PokemonBDSP_AutoStory_JubilifeCity.h"
#include "PokemonBDSP_AutoStory_Route201.h"
#include "PokemonBDSP_AutoStory_Route203.h"
#include "PokemonBDSP_AutoStory_Route204.h"
#include "PokemonBDSP_AutoStory_RavagedPath.h"
#include "PokemonBDSP_AutoStory_OreburghCity.h"
#include "PokemonBDSP_AutoStory_OreburghGym.h"
#include "PokemonBDSP_AutoStory_FloaromaTown.h"
#include "PokemonBDSP_AutoStory_SandgemTown.h"
#include "PokemonBDSP_AutoStory_Segment00Lines.h"
#include "PokemonBDSP_AutoStory_TwinleafTown.h"
#include "PokemonBDSP_AutoStory_VerityLakefront.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace RouteRecords{

using namespace Pokemon;

namespace{
const TileGrid* twinleaf(){ return &TwinleafTown::grid(); }
const TileGrid* route201(){ return &Route201::grid(); }
const TileGrid* lakefront(){ return &VerityLakefront::grid(); }
const TileGrid* sandgem(){ return &SandgemTown::grid(); }
const TileGrid* jubilife(){ return &JubilifeCity::grid(); }
const TileGrid* route203(){ return &Route203::grid(); }
const TileGrid* ravaged_path(){ return &RavagedPath::grid(); }
const TileGrid* route204_north(){ return &Route204::north_grid(); }
const TileGrid* oreburgh_city(){ return &OreburghCity::grid(); }
const TileGrid* oreburgh_gym(){ return &OreburghGym::grid(); }
const TileGrid* floaroma(){ return &FloaromaTown::grid(); }

//  00.01: the house, Twinleaf, Route 201 and the Lakefront out to the lake.
std::vector<RouteRecord> checkpoint_0001_records(){
    return {
        {"00.01", "The house's doorstep to Barry's door", "Twinleaf Town", twinleaf, true, Segment00Lines::HOUSE_DOORSTEP, {
            {Direction::WEST, 7, LegEnd::COUNT, "along row 22 to column 13, clear of the south-west house"},
            {Direction::NORTH, 10, LegEnd::COUNT, "up column 13 to row 12, below Barry's mailbox"},
            {Direction::WEST, 4, LegEnd::COUNT, "onto (9, 12), the one tile that starts Barry's scene: he bursts out of his door"},
            {Direction::NORTH, 1, LegEnd::DOOR, "after the scene, into Barry's house; the stairs, his room and the way back out are replays"},
        }, "Segment00Lines line 1, not walked by the program yet; the Twinleaf grid and the scene's tile from the Diamond decomp (land_data 0000, zone_event 0389)"},
        {"00.01", "Barry's doorstep to Route 201", "Twinleaf Town", twinleaf, true, Segment00Lines::BARRY_DOORSTEP, {
            {Direction::EAST, 6, LegEnd::COUNT, "along row 12 to column 15"},
            {Direction::NORTH, 12, LegEnd::COUNT, "up column 15 to (15, 0), the last tile in town; the guitarist's stop on row 3 is inert once Barry has talked upstairs"},
            {Direction::NORTH, 0, LegEnd::BANNER, "on north onto Route 201"},
        }, "Segment00Lines line 2, not walked by the program yet"},
        {"00.01", "Route 201: Barry's tall-grass warning, then west to the Lakefront", "Route 201", route201, true, Segment00Lines::ROUTE_201_ARRIVAL, {
            {Direction::NORTH, 6, LegEnd::COUNT, "up the exit path to (15, 18): Barry's warning fires on that row"},
            {Direction::WEST, 3, LegEnd::COUNT, "after the scene, along row 18 to column 12"},
            {Direction::NORTH, 2, LegEnd::COUNT, "to row 16, the lower level's row to the Lakefront"},
            {Direction::WEST, 12, LegEnd::COUNT, "to (0, 16), the route's last tile"},
            {Direction::WEST, 0, LegEnd::BANNER, "on west onto the Verity Lakefront"},
        }, "Segment00Lines lines 2 and 3, not walked by the program yet; traced on Route201::grid(), the decomp's since #111"},
        {"00.01", "Verity Lakefront: to below the lake's entrance", "Verity Lakefront", lakefront, true, Segment00Lines::LAKEFRONT_ARRIVAL, {
            {Direction::WEST, 11, LegEnd::COUNT, "along row 23 to column 20"},
            {Direction::NORTH, 2, LegEnd::COUNT, "to row 21"},
            {Direction::WEST, 3, LegEnd::COUNT, "to column 17, the entrance path's east column"},
            {Direction::NORTH, 7, LegEnd::COUNT, "up column 17 to (17, 14), two tiles short of the lake's scene; 00.01 ends here"},
        }, "Segment00Lines line 3, not walked by the program yet"},
    };
}

//  00.02 and 00.03: into the lake's scene, and back to Sandgem.
std::vector<RouteRecord> checkpoints_0002_0003_records(){
    return {
        {"00.02", "Into the lake's scene", "Verity Lakefront", lakefront, true, Segment00Lines::BELOW_LAKE_ENTRANCE, {
            {Direction::NORTH, 2, LegEnd::COUNT, "onto (17, 12): Barry hears Rowan and the two of us walk into Lake Verity"},
            {Direction::NORTH, 0, LegEnd::SCENE, "the starter, the Starly and the way back out to (17, 12) are the game's; nothing is walked at the lake"},
        }, "starter_nav() presses north twice from where 00.01 ends; Segment00Lines line 3's last two tiles; the scene's tiles from the Diamond decomp (zone_event 0319)"},
        {"00.03", "Verity Lakefront: from the lake's entrance back onto Route 201", "Verity Lakefront", lakefront, true, Segment00Lines::LAKE_ENTRANCE, {
            {Direction::SOUTH, 9, LegEnd::COUNT, "down column 17 to row 21"},
            {Direction::EAST, 12, LegEnd::COUNT, "along row 21 to column 29"},
            {Direction::EAST, 0, LegEnd::BANNER, "on east onto Route 201"},
        }, "Segment00Lines line 4, not walked by the program yet; which of the two entrance tiles the scene leaves us on is for the console"},
        {"00.03", "Route 201: Rowan's scene, then east through the grass to Sandgem", "Route 201", route201, true, Segment00Lines::ROUTE_201_RETURN, {
            {Direction::EAST, 5, LegEnd::COUNT, "to (5, 14): Rowan and Dawn's scene fires on that column"},
            {Direction::EAST, 12, LegEnd::COUNT, "after the scene, along row 14 to column 17"},
            {Direction::EAST, 12, LegEnd::COUNT, "through the tall grass to column 29; the Breeder wanders rows 14 to 16 there"},
            {Direction::EAST, 12, LegEnd::COUNT, "to column 41"},
            {Direction::NORTH, 7, LegEnd::COUNT, "up column 41 to row 7, beside the north-east grass"},
            {Direction::EAST, 1, LegEnd::COUNT, "into the grass"},
            {Direction::NORTH, 2, LegEnd::COUNT, "to row 5"},
            {Direction::EAST, 10, LegEnd::COUNT, "through the grass to (52, 5), its last tile"},
            {Direction::EAST, 11, LegEnd::COUNT, "to (63, 5), the route's last tile"},
            {Direction::EAST, 0, LegEnd::BANNER, "on east into Sandgem Town"},
        }, "Segment00Lines lines 4 and 5, not walked by the program yet; the grass legs need the wild-battle handling of the replay's walk_through_grass(); traced on Route201::grid()"},
        {"00.03", "Sandgem Town: Dawn at the entrance", "Sandgem Town", sandgem, true, Segment00Lines::SANDGEM_ARRIVAL, {
            {Direction::EAST, 4, LegEnd::COUNT, "along row 12 to (4, 12): Dawn's scene, which leads us into Rowan's lab for the Pokedex"},
            {Direction::EAST, 0, LegEnd::SCENE, "the lab is the game's; 00.03 ends in front of Rowan"},
        }, "Segment00Lines line 5, not walked by the program yet; on Sandgem's row 12, where its coord event (zone_event 0396) is"},
    };
}

//  01.01 to 01.05: Jubilife to Oreburgh and back out.
std::vector<RouteRecord> segment01_records_to_oreburgh(){
    return {
        {"01.01", "Jubilife Pokemon Center to Barry on Route 203", "Jubilife City", jubilife, true, std::nullopt, {
            {Direction::SOUTH, 0, LegEnd::NPC, "out of the Center's door; a girl below it stops us"},
            {Direction::WEST, 4, LegEnd::COUNT, "stick 720 ms: onto the main road", false},
            {Direction::NORTH, 16, LegEnd::COUNT, "stick 3960 ms: up to the street east to Route 203", false},
            {Direction::SOUTH, 2, LegEnd::COUNT, "stick 600 ms: back onto the street", false},
            {Direction::EAST, 0, LegEnd::NPC, "along the street onto Route 203 until Barry stops us; rows retried 0, +1, +1, -3, -1 when a wall comes first"},
        }, "jubilife_to_barry(); stick holds from the console, 2026-10-29, counted in tiles on the image grid's 3/4 rows: recount in taps before giving this record a start tile (task #21)"},
        {"01.02", "Barry to Oreburgh City through the Oreburgh Gate", "Route 203", route203, true, std::nullopt, {
            {Direction::EAST, 0, LegEnd::WALL, "along the row Barry stopped us on"},
            {Direction::NORTH, 0, LegEnd::WALL, "to the top wall, then a row south at a time until east goes on (up to ten)"},
            {Direction::EAST, 0, LegEnd::NPC, "to the Hiker"},
            {Direction::EAST, 0, LegEnd::TRANSITION, "into the Oreburgh Gate"},
            {Direction::EAST, 0, LegEnd::TRANSITION, "through the Gate into Oreburgh City"},
        }, "route203_to_oreburgh(); no start tile: Barry stops us on whichever row we came along"},
        {"01.04", "Oreburgh Pokemon Center doorstep to the Gym", "Oreburgh City", oreburgh_city, true, OreburghCity::POKEMON_CENTER_DOORSTEP, {
            {Direction::WEST, 20, LegEnd::COUNT, "D-pad taps along the Center's row to below the Gym's door"},
            {Direction::NORTH, 1, LegEnd::DOOR, "into the Gym (the fade)"},
        }, "oreburgh_street_to_gym(); 20 taps measured on the console (r204 to r215, 3 runs on the same tile); the decomp's Center door is at x 47, the taps say the doorstep is x 46"},
        {"01.04", "Inside the Oreburgh Gym", "Oreburgh Gym", oreburgh_gym, true, OreburghGym::ENTRANCE, {
            {Direction::NORTH, 0, LegEnd::NPC, "up column 5 from the entrance to Roark on row 3; the Youngsters at (4, 18) and (7, 11) see the column and stop us on rows 18 and 11 on the way"},
        }, "oreburgh_gym_battles(); the Gym's grid from the Diamond decomp (land_data 0228), its trainers and their sight from the events file (zone_event 0046)"},
        {"01.05", "Oreburgh Gym to the Gate and out onto Route 203", "Oreburgh City", oreburgh_city, true, OreburghCity::GYM_DOORSTEP, {
            {Direction::EAST, 20, LegEnd::COUNT, "D-pad taps from the Gym's doorstep back to the Center's door"},
            {Direction::WEST, 0, LegEnd::WALL, "to the stairs' cliff"},
            {Direction::NORTH, 0, LegEnd::WALL, "along the cliff"},
            {Direction::EAST, 2, LegEnd::COUNT, "to the stairs' column"},
            {Direction::NORTH, 0, LegEnd::WALL, "up the stairs"},
            {Direction::WEST, 0, LegEnd::WALL, "along the platform"},
            {Direction::SOUTH, 1, LegEnd::COUNT, "onto the Gate's row"},
            {Direction::WEST, 0, LegEnd::TRANSITION, "into the Oreburgh Gate (a row probed when a wall comes first)"},
            {Direction::WEST, 0, LegEnd::BANNER, "through the Gate until Route 203's banner"},
        }, "oreburgh_gym_to_center(), oreburgh_to_route203(); the Center visit between the two taps legs is not a walk"},
    };
}

//  01.07 to 01.11: Jubilife north to Floaroma.
std::vector<RouteRecord> segment01_records_from_jubilife(){
    return {
        {"01.07", "Jubilife's east edge to Rowan's scene", "Jubilife City", jubilife, true, std::nullopt, {
            {Direction::WEST, 14, LegEnd::COUNT, "stick 2 x 1120 ms along the long road to the crossroads with the north street", false},
            {Direction::NORTH, 0, LegEnd::SCENE, "up the street until Rowan's scene starts (about 6 s); the green-haired man may stand in the way"},
        }, "jubilife_to_rowan(); by hand 2026-10-31 (18 presses), r230b; the stick hold's count was taken on the image grid's 3/4 rows, so no start tile yet"},
        {"01.08", "Jubilife's north street onto Route 204", "Jubilife City", jubilife, true, std::nullopt, {
            {Direction::NORTH, 0, LegEnd::BANNER, "up the street until Route 204's banner; the planned walk to the Ravaged Path door follows"},
        }, "jubilife_to_ravaged_path() step 1"},
        {"01.09", "Ravaged Path's east end out onto Route 204 North", "Ravaged Path", ravaged_path, true, RavagedPath::EAST_END, {
            {Direction::WEST, 1, LegEnd::COUNT, "onto the tile above the way out"},
            {Direction::SOUTH, 0, LegEnd::TRANSITION, "out of the cave"},
        }, "ravaged_path() exit; the planned legs before it are tested as plans"},
        {"01.10", "Route 204 North's path into Floaroma", "Route 204 North", route204_north, true, TilePos{11, 5}, {
            {Direction::NORTH, 0, LegEnd::BANNER, "from (11, 5) until Floaroma Town's banner; the waypoint line before it is tested as plans"},
        }, "route204_to_floaroma(); r248a"},
        {"01.11", "Floaroma's old couple", "Floaroma Town", floaroma, true, FloaromaTown::ROUTE_204_ARRIVAL, {
            {Direction::WEST, 8, LegEnd::COUNT, "D-pad taps along the flower field's bottom row"},
            {Direction::NORTH, 0, LegEnd::NPC, "until the old woman stops us (3 tiles on r251a)"},
            {Direction::WEST, 1, LegEnd::COUNT, "below the old man"},
            {Direction::NORTH, 0, LegEnd::NPC, "until the old man stops us (a turn)"},
        }, "floaroma_old_couple(); by hand 2026-10-31 (batches m1 to m9), r251a"},
    };
}

}

//  The walks planned on a grid (00.04 and 00.05's walk_route goals, 01.03's
//  Mine legs, 01.06's Route 203 and Jubilife legs, 01.08's Route 204 South,
//  01.09's Ravaged Path, 01.10's waypoints) are data already, tested in
//  PokemonBDSP_AutoStory_Tests.cpp; these are the rest.
//
//  00.01 to 00.03 (the house, Twinleaf, Route 201, the Lakefront and the
//  lake) still replay the stick inputs Butters recorded for #1717
//  (tutorial_part_1_navigation, entry_visible, tutorial_part_2_navigation,
//  starter_nav: timed holds, diagonals among them, and "move until
//  transition"), off which no tile count can be read. Their records below
//  are the same walks as lines on the decomp grids
//  (Data/PokemonBDSP_AutoStory_Segment00Lines, task #22), legs the program
//  does not walk yet: a new-game run has to confirm where each scene leaves
//  the player before Segment_00 switches to them. The house interiors (the
//  player's, Barry's) have no grid and stay replays.
const std::vector<RouteRecord>& all(){
    static const std::vector<RouteRecord> records = []{
        std::vector<RouteRecord> all;
        for (auto part : {checkpoint_0001_records, checkpoints_0002_0003_records, segment01_records_to_oreburgh, segment01_records_from_jubilife}){
            const std::vector<RouteRecord> records = part();
            all.insert(all.end(), records.begin(), records.end());
        }
        return all;
    }();
    return records;
}


}
}
}
}
