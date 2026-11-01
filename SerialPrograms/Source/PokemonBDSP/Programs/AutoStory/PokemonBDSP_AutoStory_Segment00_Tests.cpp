/*  BDSP AutoStory Segment 00 Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_Segment00Lines.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP_AutoStory_Segment00_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::TilePos;
using Segment00Lines::SceneTrigger;
using Segment00Lines::WalkedLine;

std::string describe(const MapTile& tile){
    return tile.map + " " + Pokemon::to_string(tile.pos);
}

const SceneTrigger* find_trigger(const std::string& name){
    for (const SceneTrigger& trigger : Segment00Lines::scene_triggers()){
        if (trigger.name == name){
            return &trigger;
        }
    }
    return nullptr;
}

//  Plans one stretch of a line with the waypoint costs and returns the leg,
//  or an error. The stretch must be the one straight leg a human took: to a
//  wall or open, as the grid has it, ending exactly on the corner.
struct Stretch{
    TileLeg leg;
    std::string error;
};
Stretch plan_stretch(const StoryWorld& maps, const TilePos& from, const TilePos& to, const std::string& name){
    const auto plan = plan_robust_route(maps, std::vector<TilePos>{from},
        [&](const TilePos& pos){ return pos == to; }, waypoint_plan_costs());
    if (!plan){
        return {{}, name + ": no plan."};
    }
    if (plan->size() != 1){
        std::string text = name + ": " + std::to_string(plan->size()) + " legs, expected the one straight leg:";
        for (const PlannedLeg& leg : *plan){
            text += " " + to_string(leg.leg);
        }
        return {{}, text};
    }
    const PlannedLeg& leg = (*plan)[0];
    if (leg.after.size() != 1 || !(leg.after[0] == to)){
        return {{}, name + ": the leg " + to_string(leg.leg) + " does not end exactly on the corner."};
    }
    return {leg.leg, ""};
}

//  Every tile a line stands on, its start first, as local tiles; logs each
//  stretch's leg. Empty with `error` set when a stretch does not plan.
std::vector<MapTile> line_tiles(Logger& logger, const StoryWorld& maps, const WalkedLine& line, std::string& error){
    const std::string name = line.checkpoint + " " + line.name;
    std::vector<MapTile> tiles{line.corners[0]};
    TilePos at = maps.to_world(line.corners[0].map, line.corners[0].pos);
    std::string text = name + ": from " + describe(line.corners[0]);
    for (size_t i = 1; i < line.corners.size(); i++){
        const MapTile& corner = line.corners[i];
        const TilePos goal = maps.to_world(corner.map, corner.pos);
        const Stretch stretch = plan_stretch(maps, at, goal, name + " stretch " + std::to_string(i) + " to " + describe(corner));
        if (!stretch.error.empty()){
            error = stretch.error;
            return {};
        }
        text += "\n    " + to_string(stretch.leg) + " to " + describe(corner);
        for (size_t k = 0; k < stretch.leg.tiles; k++){
            at = at.next(stretch.leg.direction);
            const std::optional<MapTile> local = maps.to_local(at);
            if (!local){
                error = name + ": the line leaves the maps at " + Pokemon::to_string(at) + ".";
                return {};
            }
            tiles.push_back(*local);
        }
    }
    logger.log(text);
    return tiles;
}

//  The last tile steps into the line's scene; no earlier tile is in any
//  scene armed while the line is walked (an inert one is only logged).
std::string check_triggers(Logger& logger, const WalkedLine& line, const std::vector<MapTile>& tiles){
    const std::string name = line.checkpoint + " " + line.name;
    for (const std::string& live : line.live_triggers){
        if (find_trigger(live) == nullptr){
            return name + ": no scene trigger named \"" + live + "\".";
        }
    }
    const auto armed = [&](const std::string& trigger){
        return std::ranges::find(line.live_triggers, trigger) != line.live_triggers.end();
    };
    if (!line.ends_in.empty() && !armed(line.ends_in)){
        return name + ": the scene it ends in, \"" + line.ends_in + "\", is not among its live triggers.";
    }
    for (size_t i = 0; i < tiles.size(); i++){
        for (const SceneTrigger& trigger : Segment00Lines::scene_triggers()){
            if (!Segment00Lines::trigger_covers(trigger, tiles[i])){
                continue;
            }
            const bool live = armed(trigger.name);
            const bool last = i + 1 == tiles.size();
            logger.log(name + ": tile " + std::to_string(i + 1) + " of " + std::to_string(tiles.size()) + ", " + describe(tiles[i]) +
                ", is in \"" + trigger.name + "\" (" + (live ? "armed" : "inert: " + trigger.when) + ").");
            if (live && !(last && trigger.name == line.ends_in)){
                return name + ": steps into the armed scene \"" + trigger.name + "\" at " + describe(tiles[i]) + ", before its end.";
            }
        }
    }
    if (!line.ends_in.empty() && !Segment00Lines::trigger_covers(*find_trigger(line.ends_in), tiles.back())){
        return name + ": its last tile " + describe(tiles.back()) + " is not in \"" + line.ends_in + "\".";
    }
    return "";
}

//  The points the route records start and end on, each of which must lie
//  on one of the lines.
struct NamedTile{
    const char* map;            //  in segment00_maps()
    const char* json_map;
    const char* point;
};
std::vector<NamedTile> named_tiles(){
    const char* twinleaf = Segment00Lines::MAP_TWINLEAF_TOWN;
    const char* lakefront = Segment00Lines::MAP_VERITY_LAKEFRONT;
    return {
        {.map = twinleaf, .json_map = "TwinleafTown", .point = "house_doorstep"},
        {.map = twinleaf, .json_map = "TwinleafTown", .point = "barry_doorstep"},
        {.map = twinleaf, .json_map = "TwinleafTown", .point = "twinleaf_north_exit"},
        {.map = MAP_ROUTE_201, .json_map = "Route201", .point = "route_201_arrival"},
        {.map = MAP_ROUTE_201, .json_map = "Route201", .point = "barry_warning"},
        {.map = lakefront, .json_map = "VerityLakefront", .point = "lakefront_arrival"},
        {.map = lakefront, .json_map = "VerityLakefront", .point = "below_lake_entrance"},
        {.map = lakefront, .json_map = "VerityLakefront", .point = "lake_entrance"},
        {.map = MAP_ROUTE_201, .json_map = "Route201", .point = "route_201_return"},
        {.map = MAP_ROUTE_201, .json_map = "Route201", .point = "rowan_stop"},
        {.map = MAP_ROUTE_201, .json_map = "Route201", .point = "route_201_east_end"},
        {.map = MAP_SANDGEM_TOWN, .json_map = "SandgemTown", .point = "sandgem_arrival"},
        {.map = MAP_SANDGEM_TOWN, .json_map = "SandgemTown", .point = "dawn_stop"},
    };
}

//  The lines 00.01 to 00.03 would walk (Data/PokemonBDSP_AutoStory_Segment00Lines):
//  every stretch plans as one straight leg on the decomp maps, and each
//  line enters its scene's trigger rectangle on its last tile only. The
//  named tiles the route records start from all lie on the lines.
class Test_Segment00Lines : public UnitTest{
public:
    Test_Segment00Lines()
        : UnitTest("PokemonBDSP::AutoStory - Segment 00: the lines into each scene")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const StoryWorld& maps = Segment00Lines::segment00_maps();
        for (const SceneTrigger& trigger : Segment00Lines::scene_triggers()){
            if (trigger.width <= 0 || trigger.height <= 0){
                return "Scene trigger \"" + trigger.name + "\" has no tiles.";
            }
            maps.to_world(trigger.map, trigger.corner);     //  throws for a map not in the set
        }
        std::vector<MapTile> walked;
        for (const WalkedLine& line : Segment00Lines::lines()){
            if (line.corners.size() < 2){
                return line.checkpoint + " " + line.name + ": a line needs two corners.";
            }
            std::string error;
            const std::vector<MapTile> tiles = line_tiles(logger, maps, line, error);
            if (!error.empty()){
                return error;
            }
            error = check_triggers(logger, line, tiles);
            if (!error.empty()){
                return error;
            }
            walked.insert(walked.end(), tiles.begin(), tiles.end());
        }
        for (const NamedTile& tile : named_tiles()){
            const MapTile named{tile.map, map_point(tile.json_map, tile.point)};
            const bool on_a_line = std::ranges::any_of(walked, [&](const MapTile& t){
                return t.map == named.map && t.pos == named.pos;
            });
            if (!on_a_line){
                return std::string(tile.point) + " " + describe(named) + " is on none of the lines.";
            }
        }
        return true;
    }
};

}


void add_tests_AutoStory_Segment00(UnitTestDatabase& database){
    database.add<Test_Segment00Lines>();
}


}
}
}
