/*  BDSP AutoStory - Segment 00's walked lines
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "PokemonBDSP_AutoStory_SandgemTown.h"
#include "PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP_AutoStory_TwinleafTown.h"
#include "PokemonBDSP_AutoStory_VerityLakefront.h"
#include "PokemonBDSP_AutoStory_Segment00Lines.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Segment00Lines{

using namespace Pokemon;


const char* const MAP_TWINLEAF_TOWN = "Twinleaf Town";
const char* const MAP_VERITY_LAKEFRONT = "Verity Lakefront";


//  Offsets are the decomp's world coordinates of each map's tile (0, 0):
//  the Sinnoh matrix's cells are 32 tiles square, the Lakefront is cell
//  (column 2, row 26), Route 201 cells (3..4, 26), Twinleaf (3, 27) and
//  Sandgem (5, 26). SandgemTown::grid()'s rows are the game's from row 7
//  on, each the decomp's row less 4 (its header), so it sits 4 rows lower
//  than its cell; the lines touch only its row 8.
const MapSet& segment00_maps(){
    static const MapSet maps = []{
        MapSet maps;
        maps.add(MAP_TWINLEAF_TOWN, TwinleafTown::grid(), {96, 864});
        maps.add(MAP_ROUTE_201, route201_decomp_grid(), {96, 832});
        maps.add(MAP_VERITY_LAKEFRONT, VerityLakefront::grid(), {64, 832});
        maps.add(MAP_SANDGEM_TOWN, SandgemTown::grid(), {160, 836});
        return maps;
    }();
    return maps;
}


//  From the Diamond decomp's coord events (zone_event narc_0389 Twinleaf,
//  0327 Route 201, 0319 Verity Lakefront, 0396 Sandgem; Lake Verity's
//  narc_0298 has none: its scene is the map's own script, run on arrival).
//  Each is a rectangle in world tiles with the story variable and value it
//  fires at; Platinum's readable events files name the variables and carry
//  the same rectangles for Twinleaf and the Lakefront. Listed as local
//  tiles of our grids. Not here, having no grid: the player's house's door
//  tile (6, 10), where Mom's warning fires on the way out (narc_0392, value
//  1).
const std::vector<SceneTrigger>& scene_triggers(){
    static const std::vector<SceneTrigger> triggers{
        {"Barry's door", MAP_TWINLEAF_TOWN, {9, 12}, 1, 1,
            "before Barry has burst out of his house (var 0x40e6 = 0)"},
        {"the guitarist stops us leaving", MAP_TWINLEAF_TOWN, {12, 3}, 8, 1,
            "between Barry's door and his talk upstairs (var 0x4070 = 1; 2 after it)"},
        {"Barry's tall-grass warning", MAP_ROUTE_201, {14, 25}, 6, 1,
            "the first time north of Twinleaf (var 0x4086 = 0)"},
        {"Route 201's row 26 after the warning", MAP_ROUTE_201, {14, 26}, 6, 1,
            "after the warning and before the lake (var 0x4086 = 1); its script is Diamond's binary, not read"},
        {"Rowan and Dawn on the way back", MAP_ROUTE_201, {5, 21}, 1, 3,
            "after the lake (var 0x4086 = 2)"},
        {"Barry at the lake's entrance", MAP_VERITY_LAKEFRONT, {16, 12}, 2, 1,
            "the first visit (var 0x4082 = 0): he hears Rowan and the two walk into Lake Verity; the same tiles fire again at 3, later in the story"},
        {"Dawn at Sandgem's entrance", MAP_SANDGEM_TOWN, {4, 7}, 1, 5,
            "the first time in from Route 201 (var 0x4071 = 0): she leads us to Rowan's lab"},
    };
    return triggers;
}

bool trigger_covers(const SceneTrigger& trigger, const MapTile& tile){
    return tile.map == trigger.map &&
        tile.pos.x >= trigger.corner.x && tile.pos.x < trigger.corner.x + trigger.width &&
        tile.pos.y >= trigger.corner.y && tile.pos.y < trigger.corner.y + trigger.height;
}


const TilePos HOUSE_DOORSTEP{20, 22};
const TilePos BARRY_DOORSTEP{9, 12};
const TilePos TWINLEAF_NORTH_EXIT{15, 0};
const TilePos ROUTE_201_ARRIVAL{15, 31};
const TilePos BARRY_WARNING{15, 25};
const TilePos LAKEFRONT_ARRIVAL{31, 23};
const TilePos BELOW_LAKE_ENTRANCE{17, 14};
const TilePos LAKE_ENTRANCE{17, 12};
const TilePos ROUTE_201_RETURN{0, 21};
const TilePos ROWAN_STOP{5, 21};
const TilePos ROUTE_201_EAST_END{63, 12};
const TilePos SANDGEM_ARRIVAL{0, 8};
const TilePos DAWN_STOP{4, 8};

//  Corners are chosen on the maps' features (a row clear of the houses,
//  the exit path, a map's last tile, the tile a scene fires on); where a
//  straight run is longer than the planner's 12-tile open leg (the row
//  through the Lakefront, the row along Route 201) it is split at the
//  twelfth tile. The lower level of Route 201 (rows 20 to 23) is the way
//  both to and from the lake: the upper level's ledges only drop onto it.
//  Row 23 is used westward and row 21 eastward: on the way out Rowan and
//  the counterpart are not yet on (8, 21..22) and Barry is gone from (16,
//  23), but 23 keeps the line one row off the Breeder's range; on the way
//  back 21 reaches Rowan's column with the fewest corners.
//
//  What a new-game run must confirm before the segment switches to these:
//  where each scene leaves the player (Barry's warning and Rowan's scene
//  are assumed to leave us on the trigger tile; the lake's scene to warp
//  us back onto (17, 12), the right-hand entrance tile, facing south), and
//  that the stretches through tall grass are walked with the wild-battle
//  handling the replay's walk_through_grass() has, which walk_waypoints()
//  does not.
namespace{
//  Corners on each map, and a line from them: parentheses keep the table
//  flat enough for the code-health linter's brace limit.
MapTile twinleaf(const TilePos& pos){ return {MAP_TWINLEAF_TOWN, pos}; }
MapTile route201(const TilePos& pos){ return {MAP_ROUTE_201, pos}; }
MapTile lakefront(const TilePos& pos){ return {MAP_VERITY_LAKEFRONT, pos}; }
MapTile sandgem(const TilePos& pos){ return {MAP_SANDGEM_TOWN, pos}; }
WalkedLine walked(
    std::string checkpoint, std::string name, std::vector<MapTile> corners,
    std::string ends_in, std::vector<std::string> live_triggers
){
    return {std::move(checkpoint), std::move(name), std::move(corners), std::move(ends_in), std::move(live_triggers)};
}
}

const std::vector<WalkedLine>& lines(){
    static const std::vector<WalkedLine> lines{
        walked("00.01", "the house's doorstep to Barry's door", {
            twinleaf(HOUSE_DOORSTEP), twinleaf({13, 22}), twinleaf({13, 12}), twinleaf(BARRY_DOORSTEP),
        }, "Barry's door", {"Barry's door"}),
        walked("00.01", "Barry's doorstep to Route 201", {
            twinleaf(BARRY_DOORSTEP), twinleaf({15, 12}), twinleaf(TWINLEAF_NORTH_EXIT), route201(BARRY_WARNING),
        }, "Barry's tall-grass warning", {"Barry's tall-grass warning"}),
        walked("00.01", "Route 201's west half to the lake's entrance", {
            route201(BARRY_WARNING), route201({12, 25}), route201({12, 23}), route201({0, 23}),
            lakefront({20, 23}), lakefront({20, 21}), lakefront({17, 21}), lakefront(LAKE_ENTRANCE),
        }, "Barry at the lake's entrance", {"Barry at the lake's entrance", "Route 201's row 26 after the warning"}),
        walked("00.03", "the lake's entrance back to Rowan", {
            lakefront(LAKE_ENTRANCE), lakefront({17, 21}), lakefront({29, 21}), route201(ROWAN_STOP),
        }, "Rowan and Dawn on the way back", {"Rowan and Dawn on the way back"}),
        walked("00.03", "Rowan to Dawn at Sandgem's entrance", {
            route201(ROWAN_STOP), route201({17, 21}), route201({29, 21}), route201({41, 21}),
            route201({41, 14}), route201({42, 14}), route201({42, 12}), route201({52, 12}),
            route201(ROUTE_201_EAST_END), sandgem(DAWN_STOP),
        }, "Dawn at Sandgem's entrance", {"Dawn at Sandgem's entrance"}),
    };
    return lines;
}


//  Last in the file so the rows' odd count of tall-grass quotes does not
//  upset the code-health linter's measure of the functions after it (lizard
//  reads R"(...)" rows as ordinary string literals).
//  Map source: the decomp (Diamond land_data 0005 and 0006, 2026-10-31)
//      with the objects from its events file.
//  Console checks: none on these rows. Route201::grid() carries the console
//      anchors (the north-east ledge row, the Twinleaf and Sandgem gaps).
//  Status: a stand-in for the Segment 00 lines until Route201::grid() is the
//      decomp's (bdsp/maps-rederive); no checkpoint runs on it.
//
//  Decoded from Pokemon Diamond's collision table for this map (the
//  pret/pokediamond decompilation, two cells of the Sinnoh matrix:
//  files/fielddata/land_data/land_data_release/narc_0005 (column 3, row 26)
//  and narc_0006 (column 4, row 26)), with the objects from its events file
//  (zone_event narc_0327, events_route_201.json in Platinum); BDSP keeps
//  Diamond/Pearl's layout. Local (x, y) = world (x - 96, z - 832); 64 x 32
//  tiles. Twinleaf Town is the cell below (its row 0 is this map's row 32,
//  its exit columns 12 to 19 meet the path at columns 14 to 17); the Verity
//  Lakefront is the cell to the west (its column 31 is this map's column
//  -1, open on rows 14 to 18 and 20 to 23); Sandgem Town the cell to the
//  east (its column 0 is this map's column 64, open on rows 11 to 15).
//  The ledges on row 19, 'v', drop south from the upper level (rows 14 to
//  18) to the lower (rows 20 to 25): the lower level's only way east is
//  through the tall grass (20..29, 20..23), and its way back up is the gap
//  at (32..39, 19). The north-east grass, (42..52, 10..13) and (42..50,
//  14..19), is the way into Sandgem from the west. Objects, from the events
//  file:
//    arrow signposts (13, 24) and (55, 11), trainer tips signpost (40, 19):
//    'S';
//    Cashier M (44, 10), looks south and east, not a trainer (the Potions):
//    'N';
//    School Kid M (57, 15), looks west, not a trainer: 'N';
//    Pokemon Breeder M wanders around (27, 22), columns 26 to 28, rows 21
//    to 23: off the grid, the walker waits for him;
//    Lass wanders west and east from (46, 23), columns 44 to 48, row 23:
//    off the grid.
//  The scene actors are hidden until their scene and gone after it, and
//  each scene fires before its actors' tiles are reached (the triggers are
//  in scene_triggers()), so their tiles are open ground here, not 'N':
//  Barry (16, 23) (the tall-grass warning, flag 400), Prof. Rowan (8, 21)
//  and the counterpart (8, 22) (the way back, flags 406 and 407).
//  No trainers.
const TileGrid& route201_decomp_grid(){
    static const TileGrid grid({
    //     0         1         2         3         4         5         6
    //     0123456789012345678901234567890123456789012345678901234567890123
        R"(################################################################)",  //   0
        R"(################################################################)",  //   1
        R"(################################################################)",  //   2
        R"(################################################################)",  //   3
        R"(################################################################)",  //   4
        R"(################################################################)",  //   5
        R"(################################################################)",  //   6
        R"(################################################################)",  //   7
        R"(################################################################)",  //   8
        R"(################################################################)",  //   9
        R"(##########################################""N""""""""...........)",  //  10
        R"(##########################################"""""""""""..S........)",  //  11
        R"(##########################################"""""""""""...........)",  //  12
        R"(##########################################"""""""""""...........)",  //  13
        R"(....####..........######..##....##..####.."""""""""##...........)",  //  14
        R"(....####..........######..##....##..####.."""""""""##....N......)",  //  15
        R"(##........####............................"""""""""##vvvvv######)",  //  16
        R"(##........####............................"""""""""##.....######)",  //  17
        R"(.........................................."""""""""##.....######)",  //  18
        R"(vvvv####vvvvvvvvvvvvvvvvvvvvvvvv........S."""""""""##.....######)",  //  19
        R"(....####............""""""""""............................######)",  //  20
        R"(....................""""""""""............................######)",  //  21
        R"(....................""""""""""............................######)",  //  22
        R"(....................""""""""""............................######)",  //  23
        R"(########.....S......##########............................######)",  //  24
        R"(########............##########............""""""""""""""""######)",  //  25
        R"(##############....############################""""""""""""######)",  //  26
        R"(##############....############################""""""""""""######)",  //  27
        R"(##############....##############################################)",  //  28
        R"(##############....##############################################)",  //  29
        R"(##############....##############################################)",  //  30
        R"(##############....##############################################)",  //  31
    });
    return grid;
}

}
}
}
}
