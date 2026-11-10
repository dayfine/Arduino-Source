/*  BDSP AutoStory - Story table Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryTable_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;
using Pokemon::story_arg;

//  The actions each kind does itself, besides the story's events.
const std::map<StoryStepKind, std::set<std::string>>& built_in_actions(){
    static const std::map<StoryStepKind, std::set<std::string>> actions{
        {StoryStepKind::ROUTE, {"", "route", "town", "robust", "line"}},
        {StoryStepKind::LEGS, {"", "legs", "one_short_ok", "to_wall", "to_dialogue", "to_map", "counted_wall"}},
        {StoryStepKind::STEP, {"", "face", "to_banner", "tap"}},
        {StoryStepKind::MASH, {"", "a"}},
        {StoryStepKind::BATTLE, {"", "fight", "doubles", "starter"}},
        {StoryStepKind::TALK, {"", "talk_to_wanderer"}},
        {StoryStepKind::SHOP, {"", "sandgem_potions"}},
    };
    return actions;
}

//  The table: unique ids in order, each row lints, and every step's variant
//  is one its kind does or one of the story's events.
UnitTestResult test_story_table_lints(Logger&){
    const StoryEvents& events = story_events();
    std::set<std::string> ids;
    std::string previous;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (!ids.insert(row.id).second || row.id <= previous){
            return row.id + ": repeated or out of order.";
        }
        previous = row.id;
        if ((row.on_failure == StoryFailure::RELOAD_AND_RETRY) == row.saves_in.empty()){
            return row.id + ": a row names the map its start save is made in (saves_in) exactly when it saves first.";
        }
        const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(row);
        if (!problems.empty()){
            return row.id + ": " + problems[0];
        }
        for (const Pokemon::StoryStep& step : row.steps){
            const auto built_in = built_in_actions().find(step.kind);
            const bool known = events.contains(step.action) ||
                (step.kind != StoryStepKind::GAME && step.action.empty()) ||
                (built_in != built_in_actions().end() && built_in->second.contains(step.action));
            if (!known){
                return std::format("{} {}: no event or {} way named \"{}\".",
                    row.id, step.name, Pokemon::story_step_kind_name(step.kind), step.action);
            }
        }
    }
    if (!ids.contains("00.00") || !ids.contains("01.11") || story_checkpoints().front().on_failure != StoryFailure::STOP){
        return "Expected 00.00 (which stops on a failure) to 01.11.";
    }
    return true;
}

//  Where a row's last step says it leaves the player: the end of its scene,
//  or where its route (or heal) goes. None for a step that does not say.
std::optional<Pokemon::Pose> end_pose(const StoryCheckpoint& row){
    if (row.steps.empty()){
        return std::nullopt;
    }
    const Pokemon::StoryStep& last = row.steps.back();
    if (last.pose_after){
        return Pokemon::pose_of(*last.pose_after);
    }
    if (last.kind == StoryStepKind::ROUTE){
        return Pokemon::pose_of(story_arg<Pokemon::GridPose>(last.args, "target"));
    }
    return std::nullopt;
}

//  Each row starts where the row before it ends: every tile its start allows
//  is one the last row's end allows, on the same map. Checked where the last
//  step says where it ends.
UnitTestResult test_story_table_rows_join(Logger&){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    for (size_t i = 1; i < rows.size(); i++){
        const std::optional<Pokemon::Pose> end = end_pose(rows[i - 1]);
        const std::optional<Pokemon::Pose> start = rows[i].start ? std::make_optional(Pokemon::pose_of(*rows[i].start)) : std::nullopt;
        if (!end || !start){
            continue;
        }
        const std::vector<TilePos> ends = Pokemon::tiles_of(*end);
        bool joins = start->map == end->map;
        for (const TilePos& pos : Pokemon::tiles_of(*start)){
            joins = joins && std::find(ends.begin(), ends.end(), pos) != ends.end();
        }
        if (!joins){
            return std::format("{} starts at {}, but {} ends at {}.",
                rows[i].id, Pokemon::to_string(*start), rows[i - 1].id, Pokemon::to_string(*end));
        }
    }
    return true;
}

struct LadyLeg{
    Direction direction;
    size_t tiles;           //  0: walk until the wall
    TilePos end;            //  world tile
    std::string name;
};

//  00.04c's Potions: the Mart lady's walk (mart_lady()) is walks until
//  stopped, so on the map each leg ends at its wall: from a tile to three
//  below the Center's door, a tap east and north to the Center's front,
//  west to the Lab's sign, a row south and west to her grass's edge, north
//  to the trees on her row (she stands east of there), back a row south and
//  east to Route 201's sign, a row south and east to the trees east of the
//  Mart, and north to the front row's end.
std::string follow_mart_lady(const StoryWorld& maps, const std::vector<LadyLeg>& legs, TilePos at, Logger& logger){
    for (const LadyLeg& leg : legs){
        if (leg.tiles == 0){
            at = slide_to_wall(maps, at, leg.direction).stop;
        }else{
            const std::optional<TilePos> next = PokemonBDSP::step(maps, at, leg.direction);
            if (!next){
                return leg.name + ": blocked at " + describe(maps, at) + ".";
            }
            at = *next;
        }
        //  The tap east lands on whichever row we stood on.
        if (&leg != &legs.front() && !(at == leg.end)){
            return leg.name + ": ends at " + describe(maps, at) + ", not " + describe(maps, leg.end) + ".";
        }
        logger.log(leg.name + ": " + describe(maps, at));
    }
    return "";
}

UnitTestResult test_mart_lady_walk(Logger& logger){
    const StoryWorld& maps = story_maps();
    const auto sandgem = [&](const TilePos& pos){ return maps.to_world(MAP_SANDGEM_TOWN, pos); };
    const auto route201 = [&](const TilePos& pos){ return maps.to_world(MAP_ROUTE_201, pos); };
    const std::vector<LadyLeg> legs{
        {Direction::EAST, 1, sandgem({18, 0}), "a tap east under the Center's wall"},
        {Direction::NORTH, 0, sandgem({18, 11}), "north to the Center's front"},
        {Direction::WEST, 0, sandgem({6, 11}), "west to the Lab's sign"},
        {Direction::SOUTH, 1, sandgem({6, 12}), "a row south onto the path"},
        {Direction::WEST, 0, route201({42, 5}), "west to her grass's edge"},
        {Direction::NORTH, 0, route201({42, 3}), "north to the trees, on her row"},
        {Direction::SOUTH, 1, route201({42, 4}), "a row south onto the signs' row"},
        {Direction::EAST, 0, route201(map_point("Route201", "border_sign").next(Direction::WEST)), "east to Route 201's sign"},
        {Direction::SOUTH, 1, route201({54, 5}), "a row south onto the path"},
        {Direction::EAST, 0, sandgem({29, 12}), "east to the trees east of the Mart"},
        {Direction::NORTH, 0, sandgem(map_point("SandgemTown", "front_row_east_end")), "north to the front row's end"},
    };
    //  Out of the Center's door we stand a tile to three below it (r261).
    for (int below = 1; below <= 3; below++){
        const std::string error = follow_mart_lady(maps, legs, sandgem({17, 10 + below}), logger);
        if (!error.empty()){
            return std::format("From {} below the door: {}", below, error);
        }
    }
    return true;
}

//  01.10 on Route 204's north half: the line a human walked (2026-10-31),
//  corner to corner, from the Ravaged Path's exit into Floaroma. Each
//  stretch must plan as the one straight tap leg the human took, on the map
//  as it is: a detour here means the map or the line changed.
UnitTestResult test_route204_north_line(Logger& logger){
    const StoryWorld& route = route204_north_maps();
    const std::vector<TilePos> waypoints = map_region("Route204North", "floaroma_waypoints");
    struct Expected{ Direction direction; size_t tiles; };
    const std::vector<Expected> expected{
        {Direction::SOUTH, 1}, {Direction::WEST, 9}, {Direction::NORTH, 5}, {Direction::EAST, 8},
        {Direction::NORTH, 10}, {Direction::WEST, 8}, {Direction::NORTH, 7},
    };
    if (waypoints.size() != expected.size()){
        return std::format("Route 204 North: {} waypoints, expected {}.", waypoints.size(), expected.size());
    }
    std::vector<TilePos> at{map_point("Route204North", "cave_exit")};
    for (size_t i = 0; i < waypoints.size(); i++){
        const TilePos goal = waypoints[i];
        const std::string name = std::format("Route 204 North stretch {} to {}", i + 1, describe(route, goal));
        const auto plan = plan_robust_route(route, at, [&](const TilePos& pos){ return pos == goal; }, waypoint_plan_costs());
        if (!plan){
            return name + ": no plan.";
        }
        if (plan->size() != 1){
            return std::format("{}: {} legs, expected the one straight leg.", name, plan->size());
        }
        const TileLeg& leg = (*plan)[0].leg;
        logger.log(name + ": " + to_string(leg));
        if (leg.ends_at_wall || leg.direction != expected[i].direction || leg.tiles != expected[i].tiles){
            return std::format("{}: planned {}, expected an open leg of {} tiles.", name, to_string(leg), expected[i].tiles);
        }
        if ((*plan)[0].after.size() != 1 || !((*plan)[0].after[0] == goal)){
            return name + ": the leg does not end exactly on the waypoint.";
        }
        at = {goal};
    }
    if (!in_floaroma_path(at[0])){
        return "Route 204 North: the last waypoint is not in the path into Floaroma.";
    }
    return true;
}


struct AutoStoryStoryTableInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryStoryTableExpectation{
};

struct AutoStoryStoryTableCase{
    AutoStoryStoryTableInput input;
    AutoStoryStoryTableExpectation expectation;
};

class Test_AutoStoryStoryTable : public UnitTest{
public:
    explicit Test_AutoStoryStoryTable(AutoStoryStoryTableCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryStoryTableCase m_case;
};

} // namespace


const AutoStoryStoryTableCase AUTO_STORY_STORY_TABLE_CASES[] = {
    {
        .input = {"Story table: rows lint, variants known", test_story_table_lints},
        .expectation = {},
    },
    {
        .input = {"Story table: each row starts where the last one ends", test_story_table_rows_join},
        .expectation = {},
    },
    {
        .input = {"00.04: the walk to the Poke Mart lady and back", test_mart_lady_walk},
        .expectation = {},
    },
    {
        .input = {"Route 204 North: the line into Floaroma", test_route204_north_line},
        .expectation = {},
    },
};

void add_tests_AutoStory_StoryTable(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_STORY_TABLE_CASES){
        database.add<Test_AutoStoryStoryTable>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
