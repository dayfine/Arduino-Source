/*  AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plans every route of the story table on the map files, from where the
 *  table says the player stands, and checks that each checkpoint starts
 *  where the one before it ends.
 *
 */

#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_AutoStory : public UnitTest{
public:
    Test_AutoStory(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::AutoStory - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (FileException& e){
            //  CI runs with upstream Packages, which may not have the maps yet.
            return UnitTestResult(UnitTestResult::SKIPPED, "Map files not found: " + e.message());
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};

const MapData* find_map_named(const std::string& name){
    for (const MapData& map : story_maps()){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

std::string describe(const std::vector<RouteStep>& route){
    std::string text;
    for (const RouteStep& step : route){
        text += "\n  " + to_string(step);
    }
    return text;
}

std::string describe(const StoryPose& pose){
    return to_string(MapPosition{pose.map, pose.tile})
        + (pose.facing ? std::string(" facing ") + direction_name(*pose.facing) : "");
}

Direction reverse(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return Direction::SOUTH;
    case Direction::EAST:
        return Direction::WEST;
    case Direction::SOUTH:
        return Direction::NORTH;
    case Direction::WEST:
        return Direction::EAST;
    }
    return direction;
}

//  The tiles a route's walking legs pass over on `map`.
std::vector<TilePos> tiles_walked_on(const std::vector<RouteStep>& route, const std::string& map){
    std::vector<TilePos> tiles;
    for (const RouteStep& step : route){
        if (step.action != RouteAction::WALK || step.end.map != map){
            continue;
        }
        TilePos tile = step.end.point;
        for (int c = 0; c < step.tiles; c++){
            tiles.push_back(tile);
            tile = tile.next(reverse(step.direction));
        }
    }
    return tiles;
}

//  Steps that must reach Viridian Forest's gate by legs the walker can hold
//  again after a battle: each ends on a wall or a warp, or is one step.
bool held_legs_only(const std::string& name){
    return name == "forest_exit" || name == "forest_gate";
}
UnitTestResult check_held_legs(const std::string& name, const std::vector<RouteStep>& route){
    for (const RouteStep& step : route){
        if (step.action == RouteAction::WALK && !step.to_wall && !step.warps && step.tiles != 1){
            return name + ": an open leg, " + to_string(step) + ":" + describe(route);
        }
    }
    return true;
}

//  Walks `step`'s fixed legs from `pose` and moves `pose` to their end.
UnitTestResult check_legs(StoryPose& pose, const StoryStep& step){
    const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), MapPosition{pose.map, pose.tile}, step.legs);
    if (!route || route->empty()){
        return step.name + ": the legs can't be walked from " + describe(pose) + ".";
    }
    if (held_legs_only(step.name)){
        UnitTestResult result = check_held_legs(step.name, *route);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    pose = StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after};
    return true;
}

//  Plans `step` from `pose` and moves `pose` to its end.
UnitTestResult check_route(StoryPose& pose, const StoryStep& step){
    const MapPosition start{pose.map, pose.tile};
    std::optional<Direction> facing = pose.facing;
    if (!facing){
        const MapData* map = find_map_named(pose.map);
        facing = map ? wall_beside(*map, pose.tile) : std::nullopt;
        if (!facing){
            return step.name + ": the facing is unknown at " + describe(pose) + " and no wall is beside it.";
        }
    }
    const RouteGoal goal{{step.target.map, step.target.tile}, step.target.facing};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(story_maps(), start, goal, default_route_costs(), facing);
    if (!route){
        return step.name + ": no route from " + to_string(start) + " to " + to_string(goal.position) + ".";
    }
    const MapPosition end = follow_overworld_route(story_maps(), start, *route);
    if (!(end == goal.position)){
        return step.name + ": the route ends at " + to_string(end) + ":" + describe(*route);
    }
    if (goal.facing && !route->empty() && route->back().facing_after != *goal.facing){
        return step.name + ": the route doesn't end facing " + direction_name(*goal.facing) + ":" + describe(*route);
    }
    if (held_legs_only(step.name)){
        UnitTestResult result = check_held_legs(step.name, *route);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    if (step.name == "below_old_man_trigger"){
        for (const TilePos& tile : tiles_walked_on(*route, "ViridianCity")){
            if (tile == TilePos{20, 8} || tile == TilePos{22, 8}){
                return step.name + ": walks over the old man's trigger " + to_string(tile) + ":" + describe(*route);
            }
        }
    }
    pose = StoryPose{step.target.map, step.target.tile, route->empty() ? facing : route->back().facing_after};
    return true;
}

//  Follows `checkpoint` from its start; `pose` ends where it leaves the player.
UnitTestResult check_checkpoint(const StoryCheckpoint& checkpoint, StoryPose& pose){
    for (const StoryStep& step : checkpoint.steps){
        if (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS){
            UnitTestResult result = step.kind == StoryStepKind::ROUTE ? check_route(pose, step) : check_legs(pose, step);
            if (result.result != UnitTestResult::PASSED){
                return checkpoint.id + " " + result.message;
            }
        }else if (step.kind == StoryStepKind::READY){
            //  The nurse may be needed: that way must plan too, but the pose
            //  goes on from where the player stands.
            StoryPose nurse_pose = pose;
            StoryStep to_nurse = step;
            UnitTestResult result = check_route(nurse_pose, to_nurse);
            if (result.result != UnitTestResult::PASSED){
                return checkpoint.id + " " + result.message;
            }
        }else if (step.kind == StoryStepKind::STEP){
            pose = StoryPose{pose.map, pose.tile.next(step.direction), step.direction};
        }
        if (step.pose_after){
            pose = *step.pose_after;
        }
    }
    return true;
}

UnitTestResult test_routes_and_chain(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    if (checkpoints.size() != 8 || checkpoints[0].start){
        return "Expected 8 checkpoints, the first from a new game.";
    }
    StoryPose pose;
    for (size_t c = 0; c < checkpoints.size(); c++){
        const StoryCheckpoint& checkpoint = checkpoints[c];
        if (c > 0){
            const StoryPose& start = *checkpoint.start;
            const bool same_facing = !start.facing || start.facing == pose.facing;
            if (start.map != pose.map || !(start.tile == pose.tile) || !same_facing){
                return checkpoint.id + " starts at " + describe(start) + ", but " + checkpoints[c - 1].id + " ends at " + describe(pose) + ".";
            }
            pose = start;
        }
        UnitTestResult result = check_checkpoint(checkpoint, pose);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    return true;
}

//  Pewter City, its Pokemon Center and its gym, from Packages'
//  Resources/PokemonFRLG/Maps (generated by devtools/maps/frlg_maps.py), so
//  00.08's walks are planned in CI, which has no map files.
const char* const PEWTER_CITY_JSON = R"json({
"name": "PewterCity", "width": 48, "height": 40,
"tiles": [
    "################################################",
    "#######################....#####################",
    "####........#################.........##########",
    "####........#################.........##########",
    "####........#############D######......##########",
    "####........###########...............##########",
    "#######.....#####C#####..#######........########",
    "####JJJ..JJJJJJJ...#JJJJJJJJJJJJJJJJJ...########",
    "####....................................########",
    "####............................#####...########",
    "######..........................#####...########",
    "######..........................#D###...########",
    "######..................................########",
    "######...##.#######.##..................########",
    "######...#############..................########",
    "######...##.#######.##..................########",
    "######...###..#D#...##....####..........########",
    "######...............#....######........########",
    "######...............#....##D###........########",
    "######...............#.................#########",
    "######################..........................",
    "######..........................................",
    "######.......#########..........................",
    "######.......#########....##########............",
    "######.......#########....##########....########",
    "######.......####D####....###########...########",
    "######....................#.........#...########",
    "######....................#.........#...########",
    "######..#####.............#.........#...########",
    "######..#####.............#.........#...########",
    "######..#D###.......#.....#JJJJ.JJJJ#...########",
    "######..................................########",
    "######..................................########",
    "######..................................########",
    "######..................................########",
    "######..................................########",
    "####################....########################",
    "####################....########################",
    "####################....########################",
    "####################....########################"
],
"elevation": [
    "000000000000000000000000000000000000000000000000",
    "000000000000000000000003333000000000000000000000",
    "000033333333000000000000000003333333330000000000",
    "000033333333000000000000000003333333330000000000",
    "000033333333000000000000000000003333330000000000",
    "000033333333000000000003333333333333330000000000",
    "000000033333000003000003300000003333333300000000",
    "000000033000000033300000000000000000033300000000",
    "000033333333333333333333333333333333333300000000",
    "000033333333333333333333333333330000033300000000",
    "000000333333333333333333333333330000033300000000",
    "000000333333333333333333333333330000033300000000",
    "000000333333333333333333333333333333333300000000",
    "000000333003000000030033333333333333333300000000",
    "000000333000000000000033333333333333333300000000",
    "000000333003000000030033333333333333333300000000",
    "000000333000330003330033330000333333333300000000",
    "000000333333333333333033330000003333333300000000",
    "000000333333333333333033330000003333333300000000",
    "000000333333333333333033333333333333333000000000",
    "000000000000000000000033333333333333333333333333",
    "000000333333333333333333333333333333333333333333",
    "000000333333300000000033333333333333333333333333",
    "000000333333300000000033330000000000333333333333",
    "000000333333300000000033330000000000333300000000",
    "000000333333300000000033330000000000033300000000",
    "000000333333333333333333330333333333033300000000",
    "000000333333333333333333330333333333033300000000",
    "000000330000033333333333330333333333033300000000",
    "000000330000033333333333330333333333033300000000",
    "000000330000033333330333330000030000033300000000",
    "000000333333333333333333333333333333333300000000",
    "000000333333333333333333333333333333333300000000",
    "000000333333333333333333333333333333333300000000",
    "000000333333333333333333333333333333333300000000",
    "000000333333333333333333333333333333333300000000",
    "000000000000000000003333000000000000000000000000",
    "000000000000000000003333000000000000000000000000",
    "000000000000000000003333000000000000000000000000",
    "000000000000000000003333000000000000000000000000"
],
"warps": [{"x": 17, "y": 6, "dest_map": "PewterCity_Museum_1F", "dest_warp": 1}, {"x": 25, "y": 4, "dest_map": "PewterCity_Museum_1F", "dest_warp": 3}, {"x": 15, "y": 16, "dest_map": "PewterCity_Gym", "dest_warp": 1}, {"x": 28, "y": 18, "dest_map": "PewterCity_Mart", "dest_warp": 1}, {"x": 33, "y": 11, "dest_map": "", "dest_warp": 1}, {"x": 17, "y": 25, "dest_map": "PewterCity_PokemonCenter_1F", "dest_warp": 1}, {"x": 9, "y": 30, "dest_map": "", "dest_warp": 1}],
"objects": [{"x": 6, "y": 15, "kind": "lass", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 33, "y": 17, "kind": "man", "flag": "FLAG_HIDE_PEWTER_MUSEUM_GUIDE", "wander_x": 0, "wander_y": 0}, {"x": 21, "y": 28, "kind": "fat_man", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 33, "y": 27, "kind": "bug_catcher", "flag": "", "wander_x": 2, "wander_y": 0}, {"x": 42, "y": 20, "kind": "boy", "flag": "FLAG_HIDE_PEWTER_CITY_GYM_GUIDE", "wander_x": 0, "wander_y": 0}, {"x": 30, "y": 5, "kind": "cut_tree", "flag": "FLAG_TEMP_12", "wander_x": 0, "wander_y": 0}, {"x": 46, "y": 20, "kind": "scientist", "flag": "FLAG_HIDE_PEWTER_CITY_RUNNING_SHOES_GUY", "wander_x": 0, "wander_y": 0}],
"connections": [{"direction": "south", "offset": 12, "map": "Route2"}],
"points": [{"name": "gym_front", "x": 15, "y": 17}],
"flag_sets": []
})json";

const char* const PEWTER_GYM_JSON = R"json({
"name": "PewterCity_Gym", "width": 13, "height": 16,
"tiles": [
    "#############",
    "####.....####",
    "#############",
    "#....#.#....#",
    "#...........#",
    "#...#...#...#",
    "#...........#",
    "#..#.#.#.#..#",
    "##.........##",
    "#...#...#...#",
    "##.........##",
    "#...........#",
    "#...#...#...#",
    "#...........#",
    "#.....v.....#",
    "#############"
],
"elevation": [
    "0000000000000",
    "0000333330000",
    "0000000000000",
    "0333303033330",
    "0333333333330",
    "0333033303330",
    "0333333333330",
    "0330303030330",
    "0033333333300",
    "0333033303330",
    "0033333333300",
    "0333333333330",
    "0333033303330",
    "0333333333330",
    "0333333333330",
    "0000000000000"
],
"warps": [{"x": 5, "y": 14, "dest_map": "PewterCity", "dest_warp": 2}, {"x": 6, "y": 14, "dest_map": "PewterCity", "dest_warp": 2}, {"x": 7, "y": 14, "dest_map": "PewterCity", "dest_warp": 2}],
"objects": [{"x": 6, "y": 5, "kind": "brock", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 3, "y": 8, "kind": "camper", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 7, "y": 12, "kind": "gym_guy", "flag": "", "wander_x": 0, "wander_y": 0}],
"connections": [],
"points": [{"name": "brock_front", "x": 6, "y": 6}],
"flag_sets": []
})json";

const char* const PEWTER_CENTER_JSON = R"json({
"name": "PewterCity_PokemonCenter_1F", "width": 15, "height": 10,
"tiles": [
    "#.#############",
    "###############",
    "....###..##....",
    "....#######....",
    "...............",
    "...............",
    "#L.........##..",
    "...........##..",
    ".......v.......",
    "###############"
],
"elevation": [
    "030000000000000",
    "000000000000000",
    "333300033003333",
    "333300000003333",
    "333333333333333",
    "330333333333333",
    "044033333330033",
    "330333333330033",
    "333333333333333",
    "000000000000000"
],
"warps": [{"x": 6, "y": 8, "dest_map": "PewterCity", "dest_warp": 5}, {"x": 7, "y": 8, "dest_map": "PewterCity", "dest_warp": 5}, {"x": 8, "y": 8, "dest_map": "PewterCity", "dest_warp": 5}, {"x": 1, "y": 6, "dest_map": "", "dest_warp": 0}],
"objects": [{"x": 2, "y": 2, "kind": "jigglypuff", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 4, "y": 8, "kind": "gentleman", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 7, "y": 2, "kind": "nurse", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 1, "y": 2, "kind": "youngster", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 13, "y": 2, "kind": "woman_1", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 10, "y": 7, "kind": "gba_kid", "flag": "", "wander_x": 0, "wander_y": 0}, {"x": 14, "y": 6, "kind": "gba_kid", "flag": "", "wander_x": 0, "wander_y": 0}],
"connections": [],
"points": [{"name": "nurse_front", "x": 7, "y": 4}],
"flag_sets": []
})json";


const std::vector<MapData>& pewter_maps(){
    static const std::vector<MapData> maps{
        parse_map(PEWTER_CITY_JSON, "PewterCity (test fixture)"),
        parse_map(PEWTER_GYM_JSON, "PewterCity_Gym (test fixture)"),
        parse_map(PEWTER_CENTER_JSON, "PewterCity_PokemonCenter_1F (test fixture)"),
    };
    return maps;
}

struct ExpectedLeg{
    Direction direction;
    int tiles;
    TilePos end;
};

//  Plans `start` to `goal` on the Pewter maps and checks the walking legs
//  on `map`, except the step that warps onto it, against `expected`.
UnitTestResult check_legs_on(
    const MapPosition& start, Direction start_facing, const RouteGoal& goal,
    const std::string& map, const std::vector<ExpectedLeg>& expected
){
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        pewter_maps(), start, goal, default_route_costs(), start_facing
    );
    if (!route){
        return "No route from " + to_string(start) + " to " + to_string(goal.position) + ".";
    }
    std::vector<RouteStep> legs;
    for (const RouteStep& step : *route){
        if (step.action == RouteAction::WALK && !step.warps && step.end.map == map){
            legs.push_back(step);
        }
    }
    bool same = legs.size() == expected.size();
    for (size_t c = 0; same && c < legs.size(); c++){
        same = legs[c].direction == expected[c].direction && legs[c].tiles == expected[c].tiles && legs[c].end.point == expected[c].end;
    }
    if (!same){
        return "Unexpected legs on " + map + " from " + to_string(start) + " to " + to_string(goal.position) + ":" + describe(*route);
    }
    return true;
}

//  00.08, each walk from where the one before leaves the player.
UnitTestResult test_pewter_gym_walks(){
    const MapPosition nurse{"PewterCity_PokemonCenter_1F", {7, 4}};
    //  In by the door's mat (6, 14), straight up column 6 to below Liam's row.
    UnitTestResult result = check_legs_on(
        nurse, Direction::NORTH, RouteGoal{{"PewterCity_Gym", {6, 9}}, Direction::NORTH},
        "PewterCity_Gym", {{Direction::NORTH, 5, {6, 9}}}
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    //  The step onto (6, 8), into Liam's line.
    const MapData& gym = pewter_maps()[1];
    if (classify_step(gym, PlayerState{{6, 9}, elevation_at(gym, {6, 9}), false}, Direction::NORTH) != StepKind::WALK){
        return "Can't step north from (6, 9) onto (6, 8).";
    }
    //  After Liam, on to Brock: two tiles north.
    result = check_legs_on(
        MapPosition{"PewterCity_Gym", {6, 8}}, Direction::NORTH, RouteGoal{{"PewterCity_Gym", {6, 6}}, Direction::NORTH},
        "PewterCity_Gym", {{Direction::NORTH, 2, {6, 6}}}
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    //  The nurse's way: from her counter back in and up column 6 to Brock.
    return check_legs_on(
        nurse, Direction::NORTH, RouteGoal{{"PewterCity_Gym", {6, 6}}, Direction::NORTH},
        "PewterCity_Gym", {{Direction::NORTH, 8, {6, 6}}}
    );
}

UnitTestResult test_unique_names(){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (size_t i = 0; i < checkpoint.steps.size(); i++){
            for (size_t j = i + 1; j < checkpoint.steps.size(); j++){
                if (checkpoint.steps[i].name == checkpoint.steps[j].name){
                    return checkpoint.id + " has two steps named " + checkpoint.steps[i].name + ".";
                }
            }
        }
    }
    return true;
}

}


void add_tests_AutoStory(UnitTestDatabase& database){
    database.add<Test_AutoStory>("every route plans, and checkpoints chain", test_routes_and_chain);
    database.add<Test_AutoStory>("step names are unique per checkpoint", test_unique_names);
    database.add<Test_AutoStory>("Pewter Gym walks, on embedded maps", test_pewter_gym_walks);
}



}
}
}
