/*  Overworld Recovery Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  run_route() with scripted steps: walk, interruption, recovery, resume.
 *
 */

#include <format>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRecovery.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_OverworldRecovery : public UnitTest{
public:
    Test_OverworldRecovery(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::OverworldRecovery - " + std::move(name))
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

//  Steps that end as scripted, by the order they are walked in; DONE if not.
struct ScriptedRoute{
    std::map<size_t, StepEnd> ends;
    std::vector<RouteStep> walked;
    std::vector<RouteStep> held_to_warp;
    std::vector<std::string> logs;
    std::string failure;
    //  Whether the tap past a leg's wall moves the player, and the taps made.
    bool moves_past_wall = false;
    size_t wall_taps = 0;

    RouteCalls calls(){
        RouteCalls calls;
        calls.walk = [this](const RouteStep& step, std::optional<Direction>){
            walked.push_back(step);
            auto iter = ends.find(walked.size() - 1);
            return iter == ends.end() ? StepEnd::DONE : iter->second;
        };
        calls.hold_to_warp = [this](const RouteStep& step){ held_to_warp.push_back(step); };
        calls.log = [this](const std::string& message){ logs.push_back(message); };
        calls.moved_past_wall = [this](const RouteStep&){
            wall_taps++;
            return moves_past_wall;
        };
        calls.fail = [this](const std::string& message){
            failure = message;
            throw std::runtime_error(message);
        };
        return calls;
    }
    //  Runs, and returns whether it failed.
    bool run(const std::vector<RouteStep>& route, const RoutePlanning* planning){
        try{
            run_route(route, std::nullopt, planning, calls());
        }catch (std::runtime_error&){
            return true;
        }
        return false;
    }
};

std::string describe(const std::vector<RouteStep>& route){
    std::string text;
    for (const RouteStep& step : route){
        text += "\n  " + to_string(step);
    }
    return text;
}

//  Passes if each of `parts` is in some line of `logs`.
UnitTestResult expect_logged(const std::vector<std::string>& logs, const std::vector<std::string>& parts){
    for (const std::string& part : parts){
        bool found = false;
        for (const std::string& line : logs){
            found = found || line.find(part) != std::string::npos;
        }
        if (!found){
            return "Expected a log line with \"" + part + "\".";
        }
    }
    return true;
}

bool same(const RouteStep& a, const RouteStep& b){
    return to_string(a) == to_string(b);
}

//  Viridian Mart to Oak's Lab; the first open leg through grass is stopped
//  by a battle. The player is found again, walked back to that leg's end on
//  the same maps, and the rest of the route is walked as planned.
UnitTestResult test_stop_recover_resume(){
    const std::vector<MapData> maps = overworld_maps();
    const MapPosition start{"ViridianCity_Mart", map_point(maps, "ViridianCity_Mart", "clerk_front")};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps, start, RouteGoal{{"PalletTown_ProfessorOaksLab", {6, 4}}, std::nullopt}
    );
    if (!route){
        return "No route.";
    }
    size_t stopped = 0;
    while (stopped < route->size() && !((*route)[stopped].wild && !(*route)[stopped].to_wall)){
        stopped++;
    }
    if (stopped == 0 || stopped == route->size()){
        return "Expected an open leg through grass:" + describe(*route);
    }
    ScriptedRoute script;
    script.ends[stopped] = StepEnd::STOPPED;
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = std::nullopt, .label = "Route", .wall_check = WallCheck::TAP};
    if (script.run(*route, &planning)){
        return "Failed: " + script.failure;
    }

    const RouteStep& step = (*route)[stopped];
    const std::optional<Reanchor> anchor = plan_reanchor(maps, (*route)[stopped - 1].end, step);
    const std::optional<std::vector<RouteStep>> back = anchor
        ? plan_overworld_route(maps, anchor->end, RouteGoal{step.end, std::nullopt}, default_route_costs(), anchor->holds.back().direction)
        : std::nullopt;
    if (!back || !(follow_overworld_route(maps, anchor->end, *back) == step.end)){
        return "Expected a way back to " + to_string(step.end) + ".";
    }
    std::vector<RouteStep> expected(route->begin(), route->begin() + stopped + 1);
    expected.insert(expected.end(), anchor->holds.begin(), anchor->holds.end());
    expected.insert(expected.end(), back->begin(), back->end());
    expected.insert(expected.end(), route->begin() + stopped + 1, route->end());
    bool match = expected.size() == script.walked.size();
    for (size_t c = 0; match && c < expected.size(); c++){
        match = same(expected[c], script.walked[c]);
    }
    if (!match){
        return "Walked:" + describe(script.walked) + "\nexpected:" + describe(expected);
    }
    return expect_logged(script.logs, {
        "): ended at an unknown tile (unknown: stopped part-way).",
        ": found again at " + anchor->end.map + " " + Pokemon::to_string(anchor->end.point),
    });
}

//  A corridor with a person at each end: nothing finds the player again.
const char* const TEST_CORRIDOR_JSON = R"({
    "name": "Corridor", "width": 7, "height": 3,
    "tiles": ["#######", "#.....#", "#######"],
    "elevation": ["3333333", "3333333", "3333333"],
    "warps": [],
    "objects": [
        {"x": 1, "y": 1, "kind": "youngster", "flag": "", "wander_x": 0, "wander_y": 0},
        {"x": 5, "y": 1, "kind": "lass", "flag": "", "wander_x": 0, "wander_y": 0}
    ],
    "connections": [], "points": [], "flag_sets": []
})";

RouteStep walk(Direction direction, int tiles, MapPosition end){
    RouteStep step;
    step.direction = direction;
    step.tiles = tiles;
    step.facing_after = direction;
    step.end = std::move(end);
    return step;
}

//  With no maps, or no walls to find the player by, a stop fails the route
//  instead of guessing.
UnitTestResult test_no_recovery(){
    const std::vector<MapData> maps{parse_map(TEST_CORRIDOR_JSON, "Corridor")};
    const std::vector<RouteStep> route{walk(Direction::EAST, 2, {"Corridor", {4, 1}})};
    ScriptedRoute without_maps;
    without_maps.ends[0] = StepEnd::STOPPED;
    if (!without_maps.run(route, nullptr) || without_maps.failure.find("no maps") == std::string::npos){
        return "Expected a failure without maps, got: " + without_maps.failure;
    }
    ScriptedRoute without_walls;
    without_walls.ends[0] = StepEnd::STOPPED;
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = std::nullopt, .label = "Route", .wall_check = WallCheck::TAP};
    if (!without_walls.run(route, &planning) || without_walls.failure.find("no walls") == std::string::npos){
        return "Expected a failure without walls, got: " + without_walls.failure;
    }
    return true;
}

//  A leg to a wall seen interrupted after its hold is walked again; one
//  interrupted at once, again and again, fails.
UnitTestResult test_wall_legs(){
    RouteStep wall = walk(Direction::NORTH, 3, {"Corridor", {1, 1}});
    wall.to_wall = true;
    ScriptedRoute late;
    late.ends[0] = StepEnd::LATE;
    if (late.run({wall}, nullptr) || late.walked.size() != 2){
        return "Expected the leg walked again.";
    }
    ScriptedRoute blocked;
    blocked.ends[0] = StepEnd::STOPPED;
    if (!blocked.run({wall}, nullptr)){
        return "Expected a blocked leg to a wall to fail.";
    }
    return true;
}

//  Off the grass, a leg to a warp stopped by a trainer is held on to the
//  warp, and the route goes on.
UnitTestResult test_warp_leg(){
    RouteStep door = walk(Direction::SOUTH, 9, {"PewterCity", {16, 18}});
    door.warps = true;
    const RouteStep after = walk(Direction::EAST, 2, {"PewterCity", {18, 18}});
    ScriptedRoute script;
    script.ends[0] = StepEnd::STOPPED;
    if (script.run({door, after}, nullptr) || script.held_to_warp.size() != 1 || script.walked.size() != 2){
        return "Expected a hold to the warp, then the next step.";
    }
    return true;
}

//  After a leg to a wall, a tap toward it that moves the player fails the
//  route (the forest drift diagnostic); one that doesn't, or with the check
//  off, goes on. Open legs aren't tapped.
UnitTestResult test_wall_check(){
    RouteStep wall = walk(Direction::NORTH, 3, {"Corridor", {1, 1}});
    wall.to_wall = true;
    const RouteStep open = walk(Direction::EAST, 2, {"Corridor", {3, 1}});
    struct Case{
        const char* name;
        bool moves;
        WallCheck check;
        bool fails;
        size_t taps;
    };
    const Case cases[] = {
        {.name = "the tap moves", .moves = true, .check = WallCheck::TAP, .fails = true, .taps = 1},
        {.name = "the tap bumps", .moves = false, .check = WallCheck::TAP, .fails = false, .taps = 1},
        {.name = "the check is off", .moves = true, .check = WallCheck::SKIP, .fails = false, .taps = 0},
    };
    for (const Case& c : cases){
        const RoutePlanning planning{.maps = nullptr, .costs = default_route_costs(), .start = std::nullopt, .label = "Route", .wall_check = c.check};
        ScriptedRoute script;
        script.moves_past_wall = c.moves;
        const bool failed = script.run({wall, open}, &planning);
        if (failed != c.fails || script.wall_taps != c.taps){
            return std::format("{}: expected {} after {} tap(s), got {} after {}: {}",
                c.name, c.fails ? "a failure" : "no failure", c.taps, failed ? "a failure" : "none", script.wall_taps, script.failure);
        }
        if (c.fails && script.failure != "position check: " + to_string(wall) + " did not end at its wall"){
            return std::string(c.name) + ": wrong failure: " + script.failure;
        }
    }
    return true;
}

//  The route log lines the games share (Pokemon_RouteLog.h): the plan from
//  the caller's start under its label, then each leg and how it ended.
UnitTestResult test_route_logged(){
    const std::vector<RouteStep> route{
        walk(Direction::EAST, 2, {"Corridor", {4, 1}}),
        walk(Direction::WEST, 1, {"Corridor", {3, 1}}),
    };
    const RoutePlanning planning{.maps = nullptr, .costs = default_route_costs(), .start = MapPosition{"Corridor", {2, 1}}, .label = "test walk", .wall_check = WallCheck::TAP};
    ScriptedRoute script;
    if (script.run(route, &planning)){
        return "Failed: " + script.failure;
    }
    const std::vector<std::string> expected{
        "[Route] test walk: plan from Corridor (2, 1):\n"
        "    east 2 (open) to Corridor (4, 1)\n"
        "    west 1 (open) to Corridor (3, 1)",
        "[Route] Leg 1/2: east 2 (open) to Corridor (4, 1)",
        "[Route] test walk: east 2 (open): ended at Corridor (4, 1) (walked).",
        "[Route] Leg 2/2: west 1 (open) to Corridor (3, 1)",
        "[Route] test walk: west 1 (open): ended at Corridor (3, 1) (walked).",
    };
    for (size_t c = 0; c < expected.size(); c++){
        const std::string got = c < script.logs.size() ? "\"" + script.logs[c] + "\"" : std::string("nothing");
        if (c >= script.logs.size() || script.logs[c] != expected[c]){
            return "Log line " + std::to_string(c) + ": expected \"" + expected[c] + "\", got " + got + ".";
        }
    }
    return true;
}

}


void add_tests_OverworldRecovery(UnitTestDatabase& database){
    database.add<Test_OverworldRecovery>("stop, recover, resume", test_stop_recover_resume);
    database.add<Test_OverworldRecovery>("no recovery", test_no_recovery);
    database.add<Test_OverworldRecovery>("legs to a wall", test_wall_legs);
    database.add<Test_OverworldRecovery>("a leg to a warp", test_warp_leg);
    database.add<Test_OverworldRecovery>("the route is logged", test_route_logged);
    database.add<Test_OverworldRecovery>("a leg to a wall ends at it", test_wall_check);
}



}
}
}
