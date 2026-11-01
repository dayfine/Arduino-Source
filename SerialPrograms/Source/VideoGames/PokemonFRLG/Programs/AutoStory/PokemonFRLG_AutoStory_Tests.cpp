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

//  A table pose as a StoryPose; FRLG's table has one-tile poses only.
StoryPose one_tile(const Pokemon::Pose& pose){
    const std::optional<StoryPose> ret = story_pose(pose);
    if (!ret){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Not a one-tile pose: " + Pokemon::to_string(pose));
    }
    return *ret;
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
    if (!step.target){
        return step.name + ": no target.";
    }
    const StoryPose target = one_tile(*step.target);
    const RouteGoal goal{{target.map, target.tile}, target.facing};
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
    pose = StoryPose{target.map, target.tile, route->empty() ? facing : route->back().facing_after};
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
            pose = one_tile(*step.pose_after);
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
            const StoryPose start = one_tile(*checkpoint.start);
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

struct ExpectedLeg{
    Direction direction;
    int tiles;
    TilePos end;
};

//  Plans `start` to `goal` on the story maps and checks the walking legs
//  on `map`, except the step that warps onto it, against `expected`.
UnitTestResult check_legs_on(
    const MapPosition& start, Direction start_facing, const RouteGoal& goal,
    const std::string& map, const std::vector<ExpectedLeg>& expected
){
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        story_maps(), start, goal, default_route_costs(), start_facing
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
    const MapData& gym = *find_map_named("PewterCity_Gym");
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

UnitTestResult test_lint(){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(checkpoint);
        if (!problems.empty()){
            return problems[0];
        }
    }
    return true;
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
    database.add<Test_AutoStory>("every checkpoint lints clean", test_lint);
    database.add<Test_AutoStory>("Pewter Gym walks", test_pewter_gym_walks);
}



}
}
}
