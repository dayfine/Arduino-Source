/*  BDSP AutoStory - Segment 00's walked lines
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP_AutoStory_Segment00Lines.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Segment00Lines{

using Pokemon::TilePos;


const char* const MAP_TWINLEAF_TOWN = "Twinleaf Town";
const char* const MAP_VERITY_LAKEFRONT = "Verity Lakefront";


//  Offsets are the decomp's world coordinates of each map's tile (0, 0):
//  the Sinnoh matrix's cells are 32 tiles square, the Lakefront is cell
//  (column 2, row 26), Route 201 cells (3..4, 26), Twinleaf (3, 27) and
//  Sandgem (5, 26). The Route201 map leaves off the cell's seven wall rows
//  at the top, so it starts 7 rows lower; story_map("SandgemTown") is its whole
//  cell.
const StoryWorld& segment00_maps(){
    static const StoryWorld maps = []{
        StoryWorld maps;
        maps.add(MAP_TWINLEAF_TOWN, story_map("TwinleafTown"), {96, 864});
        maps.add(MAP_ROUTE_201, story_map("Route201"), {96, 839});
        maps.add(MAP_VERITY_LAKEFRONT, story_map("VerityLakefront"), {64, 832});
        maps.add(MAP_SANDGEM_TOWN, story_map("SandgemTown"), {160, 832});
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
        {"Barry's tall-grass warning", MAP_ROUTE_201, {14, 18}, 6, 1,
            "the first time north of Twinleaf (var 0x4086 = 0)"},
        {"Route 201's row 19 after the warning", MAP_ROUTE_201, {14, 19}, 6, 1,
            "after the warning and before the lake (var 0x4086 = 1); its script is Diamond's binary, not read"},
        {"Rowan and Dawn on the way back", MAP_ROUTE_201, {5, 14}, 1, 3,
            "after the lake (var 0x4086 = 2)"},
        {"Barry at the lake's entrance", MAP_VERITY_LAKEFRONT, {16, 12}, 2, 1,
            "the first visit (var 0x4082 = 0): he hears Rowan and the two walk into Lake Verity; the same tiles fire again at 3, later in the story"},
        {"Dawn at Sandgem's entrance", MAP_SANDGEM_TOWN, {4, 11}, 1, 5,
            "the first time in from Route 201 (var 0x4071 = 0): she leads us to Rowan's lab"},
    };
    return triggers;
}

bool trigger_covers(const SceneTrigger& trigger, const MapTile& tile){
    return tile.map == trigger.map &&
        tile.pos.x >= trigger.corner.x && tile.pos.x < trigger.corner.x + trigger.width &&
        tile.pos.y >= trigger.corner.y && tile.pos.y < trigger.corner.y + trigger.height;
}



//  Corners are chosen on the maps' features (a row clear of the houses,
//  the exit path, a map's last tile, the tile a scene fires on); where a
//  straight run is longer than the planner's 12-tile open leg (the row
//  through the Lakefront, the row along Route 201) it is split at the
//  twelfth tile. The lower level of Route 201 (rows 13 to 16) is the way
//  both to and from the lake: the upper level's ledges only drop onto it.
//  Row 16 is used westward and row 14 eastward: on the way out Rowan and
//  the counterpart are not yet on (8, 14..15) and Barry is gone from (16,
//  16), but 16 keeps the line one row off the Breeder's range; on the way
//  back 14 reaches Rowan's column with the fewest corners.
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
//  The same, at a named point of the map.
MapTile twinleaf_point(const char* point){ return twinleaf(map_point("TwinleafTown", point)); }
MapTile route201_point(const char* point){ return route201(map_point("Route201", point)); }
MapTile lakefront_point(const char* point){ return lakefront(map_point("VerityLakefront", point)); }
MapTile sandgem_point(const char* point){ return sandgem(map_point("SandgemTown", point)); }
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
            twinleaf_point("house_doorstep"), twinleaf({13, 22}), twinleaf({13, 12}), twinleaf_point("barry_doorstep"),
        }, "Barry's door", {"Barry's door"}),
        walked("00.01", "Barry's doorstep to Route 201", {
            twinleaf_point("barry_doorstep"), twinleaf({15, 12}), twinleaf_point("twinleaf_north_exit"), route201_point("barry_warning"),
        }, "Barry's tall-grass warning", {"Barry's tall-grass warning"}),
        walked("00.01", "Route 201's west half to the lake's entrance", {
            route201_point("barry_warning"), route201({14, 18}), route201({14, 17}), route201({12, 17}), route201({12, 16}), route201({0, 16}),
            lakefront({20, 23}), lakefront({20, 21}), lakefront({17, 21}), lakefront_point("lake_entrance"),
        }, "Barry at the lake's entrance", {"Barry at the lake's entrance", "Route 201's row 19 after the warning"}),
        walked("00.03", "the lake's entrance back to Rowan", {
            lakefront_point("lake_entrance"), lakefront({17, 21}), lakefront({29, 21}), route201_point("rowan_stop"),
        }, "Rowan and Dawn on the way back", {"Rowan and Dawn on the way back"}),
        walked("00.03", "Rowan to Dawn at Sandgem's entrance", {
            route201_point("rowan_stop"), route201({17, 14}), route201({29, 14}), route201({41, 14}),
            route201({41, 7}), route201({42, 7}), route201({42, 5}), route201({52, 5}),
            route201_point("route_201_east_end"), sandgem_point("dawn_stop"),
        }, "Dawn at Sandgem's entrance", {"Dawn at Sandgem's entrance"}),
    };
    return lines;
}

}
}
}
}
