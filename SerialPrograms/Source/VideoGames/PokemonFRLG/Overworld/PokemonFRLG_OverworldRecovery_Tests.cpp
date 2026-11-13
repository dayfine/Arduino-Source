/*  Overworld Recovery Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  run_route() with scripted steps: walk, interruption, recovery, resume.
 *
 */

#include <algorithm>
#include <chrono>
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

struct OverworldRecoveryInput{
    std::string name;
    UnitTestResult (*body)();
};

struct OverworldRecoveryExpectation{
    std::string_view description;
};

struct OverworldRecoveryTestCase{
    OverworldRecoveryInput input;
    OverworldRecoveryExpectation expectation;
};

class Test_OverworldRecovery : public UnitTest{
public:
    explicit Test_OverworldRecovery(OverworldRecoveryTestCase scenario)
        : UnitTest("PokemonFRLG::OverworldRecovery - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_scenario.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    OverworldRecoveryTestCase m_scenario;
};

//  Steps that end as scripted, by the order they are walked in; DONE if not.
struct ScriptedRoute{
    std::map<size_t, StepOutcome> ends;
    std::vector<RouteStep> walked;
    std::vector<RouteStep> held_to_warp;
    std::vector<std::string> logs;
    std::string failure;
    //  Whether the tap past a leg's wall moves the player, and the taps made.
    bool moves_past_wall = false;
    size_t wall_taps = 0;
    //  The way each walked step started facing.
    std::vector<std::optional<Direction>> facings;
    //  How the watches for a trainer end, in order; DONE after these. The
    //  approach of each watch made.
    std::vector<StepEnd> watch_ends;
    std::vector<int> watches;

    RouteCalls calls(){
        RouteCalls calls;
        calls.walk = [this](const RouteStep& step, std::optional<Direction> facing){
            walked.push_back(step);
            facings.push_back(facing);
            auto iter = ends.find(walked.size() - 1);
            return iter == ends.end() ? StepOutcome{} : iter->second;
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
        calls.watch_for_trainer = [this](const RouteStep&, int approach_tiles){
            watches.push_back(approach_tiles);
            return watches.size() <= watch_ends.size() ? watch_ends[watches.size() - 1] : StepEnd::DONE;
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

UnitTestResult expect_walked(const std::vector<RouteStep>& expected, const std::vector<RouteStep>& walked){
    bool match = expected.size() == walked.size();
    for (size_t c = 0; match && c < expected.size(); c++){
        match = same(expected[c], walked[c]);
    }
    if (!match){
        return "Walked:" + describe(walked) + "\nexpected:" + describe(expected);
    }
    return true;
}

StepOutcome stop_part_way(std::optional<WalkedTiles> walked = std::nullopt){
    return StepOutcome{.end = StepEnd::STOPPED, .walked = walked};
}

//  Viridian Mart to Oak's Lab; the first open leg through grass is stopped
//  by a battle halfway, by its timing. The player is found again from the
//  tiles around there, walked back to that leg's end, and the rest of the
//  route is walked as planned.
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
    const RouteStep& step = (*route)[stopped];
    const WalkedTiles walked = walked_near(step, step.tiles / 2, std::chrono::milliseconds(267));
    ScriptedRoute script;
    script.ends[stopped] = StepOutcome{.end = StepEnd::STOPPED, .walked = walked};
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = std::nullopt, .label = "Route", .wall_check = WallCheck::TAP};
    if (script.run(*route, &planning)){
        return "Failed: " + script.failure;
    }

    const std::vector<StopCandidate> candidates = stop_candidates(maps, (*route)[stopped - 1].end, step, walked);
    const std::optional<Reanchor> anchor = plan_reanchor(maps, candidates, step);
    if (candidates.size() != (size_t)(walked.max - walked.min + 1) || !anchor || anchor->holds.empty()){
        return std::format("Expected holds from the {} tiles around the estimate.", walked.max - walked.min + 1);
    }
    std::vector<RouteStep> expected(route->begin(), route->begin() + stopped + 1);
    expected.insert(expected.end(), anchor->holds.begin(), anchor->holds.end());
    expected.insert(expected.end(), anchor->back.begin(), anchor->back.end());
    expected.insert(expected.end(), route->begin() + stopped + 1, route->end());
    const UnitTestResult walked_as_expected = expect_walked(expected, script.walked);
    if (walked_as_expected.result != UnitTestResult::PASSED){
        return walked_as_expected;
    }
    return expect_logged(script.logs, {
        "): ended at an unknown tile (unknown: stopped part-way).",
        std::format("{} stopped {} to {} tiles in; the player is on one of: {};", to_string(step), walked.min, walked.max, to_string(candidates[0])),
        ": found again at " + anchor->found.map + " " + Pokemon::to_string(anchor->found.point),
    });
}

//  A corridor with a person at each end: nothing finds the player again.
const std::string TEST_CORRIDOR_JSON = R"({
    "name": "Corridor", "source": {"decomp_file": "", "revision": "test", "date": "2026-10-31"},
    "tiles": ["#######", "#.....#", "#######"],
    "elevation": ["3333333", "3333333", "3333333"],
    "objects": [
        {"name": "youngster", "kind": "npc", "x": 1, "y": 1},
        {"name": "lass", "kind": "npc", "x": 5, "y": 1}
    ]
})";

RouteStep walk(Direction direction, int tiles, MapPosition end){
    RouteStep step;
    step.direction = direction;
    step.tiles = tiles;
    step.facing_after = direction;
    step.end = std::move(end);
    return step;
}

//  With no maps, or no walls to find the player by from every tile they may
//  be on, a stop fails the route instead of guessing.
UnitTestResult test_no_recovery(){
    const std::vector<MapData> maps{parse_map(TEST_CORRIDOR_JSON, "Corridor")};
    const std::vector<RouteStep> route{walk(Direction::EAST, 2, {"Corridor", {4, 1}})};
    ScriptedRoute without_maps;
    without_maps.ends[0] = stop_part_way();
    if (!without_maps.run(route, nullptr) || without_maps.failure.find("no maps") == std::string::npos){
        return "Expected a failure without maps, got: " + without_maps.failure;
    }
    ScriptedRoute without_walls;
    without_walls.ends[0] = stop_part_way(WalkedTiles{.min = 1, .max = 2});
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = std::nullopt, .label = "Route", .wall_check = WallCheck::TAP};
    if (!without_walls.run(route, &planning) || without_walls.failure.find("no holds to walls find the player") == std::string::npos){
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
    late.ends[0] = StepOutcome{.end = StepEnd::LATE, .walked = std::nullopt};
    if (late.run({wall}, nullptr) || late.walked.size() != 2){
        return "Expected the leg walked again.";
    }
    ScriptedRoute blocked;
    blocked.ends[0] = stop_part_way();
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
    script.ends[0] = stop_part_way();
    if (script.run({door, after}, nullptr) || script.held_to_warp.size() != 1 || script.walked.size() != 2){
        return "Expected a hold to the warp, then the next step.";
    }
    return true;
}

//  After a leg to a wall, a tap toward it that moves the screen is only
//  logged (the forest drift diagnostic): a bump and a step can't be told
//  apart yet. With the check off there is no tap. Open legs aren't tapped.
UnitTestResult test_wall_check(){
    RouteStep wall = walk(Direction::NORTH, 3, {"Corridor", {1, 1}});
    wall.to_wall = true;
    const RouteStep open = walk(Direction::EAST, 2, {"Corridor", {3, 1}});
    struct Case{
        std::string_view name;
        bool moves;
        WallCheck check;
        bool logged;
        size_t taps;
    };
    const Case cases[] = {
        {.name = "the tap moves", .moves = true, .check = WallCheck::TAP, .logged = true, .taps = 1},
        {.name = "the tap bumps", .moves = false, .check = WallCheck::TAP, .logged = false, .taps = 1},
        {.name = "the check is off", .moves = true, .check = WallCheck::SKIP, .logged = false, .taps = 0},
    };
    for (const Case& c : cases){
        const RoutePlanning planning{.maps = nullptr, .costs = default_route_costs(), .start = std::nullopt, .label = "Route", .wall_check = c.check};
        ScriptedRoute script;
        script.moves_past_wall = c.moves;
        if (script.run({wall, open}, &planning) || script.walked.size() != 2 || script.wall_taps != c.taps){
            return std::format("{}: expected both legs walked after {} tap(s), got {} after {}: {}",
                c.name, c.taps, script.walked.size(), script.wall_taps, script.failure);
        }
        const UnitTestResult logged = expect_logged(script.logs, {"Position check (logged only): a tap after " + to_string(wall)});
        if ((logged.result == UnitTestResult::PASSED) != c.logged){
            return std::string(c.name) + (c.logged ? ": expected the tap logged." : ": didn't expect the tap logged.");
        }
    }
    return true;
}

//  A hall to walk: floor from x=1 to x=7, nothing in it.
const std::string TEST_HALL_JSON = R"({
    "name": "Hall", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-12"},
    "tiles": ["#########", "#.......#", "#########"],
    "elevation": ["333333333", "333333333", "333333333"],
    "objects": []
})";

StepOutcome no_movement(){
    return StepOutcome{.end = StepEnd::DONE, .walked = std::nullopt, .movement = HoldMovement::NONE};
}

//  A leg held from a known tile that moved the screen not at all did not
//  walk (console B, 2026-11-12, 12.03 pallet_shore): it is not taken as
//  walked, whether open or to a wall, but walked again from where it
//  started; once more with nothing moved, the route fails. A leg that
//  moved is walked as before.
UnitTestResult test_unmoved_leg(){
    const std::vector<MapData> maps{parse_map(TEST_HALL_JSON, "Hall")};
    RouteStep wall = walk(Direction::EAST, 5, {"Hall", {7, 1}});
    wall.to_wall = true;
    const std::vector<RouteStep> legs[] = {
        {walk(Direction::EAST, 3, {"Hall", {5, 1}})},
        {wall},
    };
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = MapPosition{"Hall", {2, 1}}, .label = "Route", .wall_check = WallCheck::SKIP};
    for (const std::vector<RouteStep>& route : legs){
        const std::string name = to_string(route[0]);
        ScriptedRoute once;
        once.ends[0] = no_movement();
        if (once.run(route, &planning) || once.walked.size() != 2){
            return name + ": expected the leg walked a second time after it moved nothing: " + once.failure;
        }
        const UnitTestResult logged = expect_logged(once.logs, {"the player never moved", "stopped 0 to 0 tiles in"});
        if (logged.result != UnitTestResult::PASSED){
            return name + ": " + logged.message;
        }
        ScriptedRoute twice;
        twice.ends[0] = no_movement();
        twice.ends[1] = no_movement();
        if (!twice.run(route, &planning) || twice.failure.find("moved nothing twice in a row") == std::string::npos){
            return name + ": expected a failure after two holds that moved nothing, got: " + twice.failure;
        }
        ScriptedRoute moved;
        moved.ends[0] = StepOutcome{.end = StepEnd::DONE, .walked = std::nullopt, .movement = HoldMovement::MOVED};
        if (moved.run(route, &planning) || moved.walked.size() != 1){
            return name + ": a leg that moved is walked once: " + moved.failure;
        }
        ScriptedRoute unmeasured;
        if (unmeasured.run(route, &planning) || unmeasured.walked.size() != 1){
            return name + ": a leg not measured is walked once: " + unmeasured.failure;
        }
    }
    return true;
}

//  A hold that finds the player starts from one of a few tiles. If one of
//  them stands at its wall, a hold that moved nothing may be a bump right
//  away and is taken as walked; if none does, it never moved, and as the
//  tile it was held from isn't known, the route fails.
UnitTestResult test_unmoved_hold_from_unknown_tile(){
    const std::vector<MapData> maps{parse_map(TEST_HALL_JSON, "Hall")};
    const RouteStep open = walk(Direction::EAST, 5, {"Hall", {7, 1}});
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = MapPosition{"Hall", {2, 1}}, .label = "Route", .wall_check = WallCheck::SKIP};
    struct Case{
        std::string_view name;
        WalkedTiles stopped;
        bool bump;
    };
    //  Walked 4 to 5 tiles stops on (6, 1) or (7, 1), the wall's tile; 2 to
    //  3 on (4, 1) or (5, 1).
    const Case cases[] = {
        {.name = "one tile at the wall", .stopped = WalkedTiles{.min = 4, .max = 5}, .bump = true},
        {.name = "none at the wall", .stopped = WalkedTiles{.min = 2, .max = 3}, .bump = false},
    };
    for (const Case& c : cases){
        ScriptedRoute script;
        script.ends[0] = stop_part_way(c.stopped);
        script.ends[1] = no_movement();
        const bool failed = script.run({open}, &planning);
        if (script.walked.size() < 2 || !script.walked[1].to_wall){
            return std::format("{}: expected a hold to a wall after the stop:{}", c.name, describe(script.walked));
        }
        if (c.bump){
            const UnitTestResult logged = expect_logged(script.logs, {"as a bump into the wall it may stand at already"});
            if (failed || logged.result != UnitTestResult::PASSED){
                return std::format("{}: expected the hold taken as a bump: {}", c.name, script.failure);
            }
        }else if (!failed || script.failure.find("moved nothing") == std::string::npos){
            return std::format("{}: expected a failure, got: {}", c.name, script.failure);
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
        "[Route] test walk: plan from Corridor (2, 1): east 2 (open) to Corridor (4, 1); west 1 (open) to Corridor (3, 1)",
        "[Route] Leg 1/2: east 2 (open) to Corridor (4, 1)",
        "[Route] test walk: east 2 (open): ended at Corridor (4, 1) (walked).",
        "[Route] Leg 2/2: west 1 (open) to Corridor (3, 1)",
        "[Route] test walk: west 1 (open): ended at Corridor (3, 1) (walked).",
    };
    //  The [Leg] records are the replay's; they are checked apart.
    std::vector<std::string> logs;
    size_t leg_records = 0;
    for (const std::string& line : script.logs){
        if (line.starts_with("[Leg] ")){
            leg_records++;
        }else{
            logs.push_back(line);
        }
    }
    if (leg_records != route.size()){
        return "Expected one [Leg] record per leg, got " + std::to_string(leg_records) + ".";
    }
    for (size_t c = 0; c < expected.size(); c++){
        const std::string got = c < logs.size() ? "\"" + logs[c] + "\"" : std::string("nothing");
        if (c >= logs.size() || logs[c] != expected[c]){
            return "Log line " + std::to_string(c) + ": expected \"" + expected[c] + "\", got " + got + ".";
        }
    }
    return true;
}

//  Route 3's Lass Robin (console B, 2026-11-02), shrunk: a corridor along
//  row 1, walled at x 6, and below it Robin's column x 3, where she wanders
//  y 2..4 looking north and south, sight 3. Every way east along the
//  corridor crosses her sight at (3, 1), and nowhere else.
const std::string TEST_ROBIN_JSON = R"({
    "name": "Robin", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-02"},
    "tiles": ["#######", "......#", "###.###", "###.###", "###.###", "#######"],
    "objects": [
        {"name": "lass", "kind": "npc", "x": 3, "y": 3, "wander": {"x0": 3, "y0": 2, "x1": 3, "y1": 4}, "trainer": {"sight": 3, "looks": ["north", "south"]}}
    ]
})";

MapPosition robin(int x, int y){
    return MapPosition{"Robin", {x, y}};
}

//  East along the corridor crosses Robin's sight on (3, 1) only; west of
//  her, or back from the wall, it doesn't.
UnitTestResult test_trainer_crossing(){
    const std::vector<MapData> maps{parse_map(TEST_ROBIN_JSON, "Robin")};
    RouteStep east = walk(Direction::EAST, 5, robin(5, 1));
    east.to_wall = true;
    const std::optional<TrainerCrossing> crossing = trainer_crossing(maps, robin(0, 1), east);
    if (!crossing || crossing->seen != std::vector<MapPosition>{robin(3, 1)} || crossing->approach_tiles != 3){
        return "Expected east along the corridor to cross her sight on (3, 1) only, with an approach of 3.";
    }
    if (trainer_crossing(maps, robin(0, 1), walk(Direction::EAST, 2, robin(2, 1)))
        || trainer_crossing(maps, robin(5, 1), walk(Direction::WEST, 1, robin(4, 1)))
    ){
        return "Expected legs off her column to cross no sight.";
    }
    return true;
}

//  A leg east past Robin, then a turn north: how the leg and the watch for
//  her after it end, and whether she saw the player.
struct TrainerCase{
    std::string_view name;
    RouteStep leg;
    StepEnd walk_end;
    std::optional<WalkedTiles> walked;
    StepEnd watch_end;
    size_t watches;
    bool seen;
};

//  What `c`'s route should walk: the leg, then, if she saw the player, the
//  way on from (3, 1) to its end, then the turn.
std::optional<std::vector<RouteStep>> expected_walk(const std::vector<MapData>& maps, const TrainerCase& c, const RouteStep& face){
    std::vector<RouteStep> expected{c.leg};
    if (c.seen){
        const std::optional<std::vector<RouteStep>> way = plan_overworld_route(maps, robin(3, 1), RouteGoal{c.leg.end, std::nullopt}, default_route_costs(), Direction::EAST);
        if (!way || way->empty()){
            return std::nullopt;
        }
        expected.insert(expected.end(), way->begin(), way->end());
    }
    expected.push_back(face);
    return expected;
}

UnitTestResult run_trainer_case(const std::vector<MapData>& maps, const TrainerCase& c){
    RouteStep face = walk(Direction::NORTH, 0, c.leg.end);
    face.action = RouteAction::FACE;
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = robin(0, 1), .label = "Route", .wall_check = WallCheck::SKIP};
    ScriptedRoute script;
    script.ends[0] = StepOutcome{.end = c.walk_end, .walked = c.walked};
    script.watch_ends = {c.watch_end};
    if (script.run({c.leg, face}, &planning)){
        return "failed: " + script.failure;
    }
    const std::optional<std::vector<RouteStep>> expected = expected_walk(maps, c, face);
    if (!expected){
        return "expected a way on from (3, 1).";
    }
    bool match = expected->size() == script.walked.size() && script.watches.size() == c.watches;
    for (size_t i = 0; match && i < expected->size(); i++){
        match = same((*expected)[i], script.walked[i]);
    }
    if (!match){
        return std::format("walked:{}\nexpected:{}\nafter {} watch(es)", describe(script.walked), describe(*expected), script.watches.size());
    }
    if (c.watches > 0 && script.watches[0] != 3){
        return "expected a watch for an approach of 3.";
    }
    if (c.seen && script.facings[1] != Direction::EAST){
        return "expected the route on from (3, 1) to start facing east.";
    }
    const UnitTestResult logged = expect_logged(script.logs, {"stopped by a trainer who saw the player, at Robin (3, 1); planning again."});
    if ((logged.result == UnitTestResult::PASSED) != c.seen){
        return c.seen ? "expected the stop logged at (3, 1)." : "didn't expect a stop logged.";
    }
    return true;
}

//  She sees the player: the leg's watch for her (or the leg itself) is
//  interrupted by her message, which the interrupts fight, and the route is
//  planned again from (3, 1), facing east. When she doesn't, the route is
//  walked as planned, after one watch.
UnitTestResult test_trainer_stops_leg(){
    const std::vector<MapData> maps{parse_map(TEST_ROBIN_JSON, "Robin")};
    RouteStep wall = walk(Direction::EAST, 5, robin(5, 1));
    wall.to_wall = true;
    const RouteStep open = walk(Direction::EAST, 4, robin(4, 1));
    const TrainerCase cases[] = {
        {.name = "to a wall, seen", .leg = wall, .walk_end = StepEnd::DONE, .walked = std::nullopt, .watch_end = StepEnd::LATE, .watches = 1, .seen = true},
        {.name = "open, seen", .leg = open, .walk_end = StepEnd::STOPPED, .walked = std::nullopt, .watch_end = StepEnd::DONE, .watches = 0, .seen = true},
        //  She stops the player on (3, 1) and speaks only after walking up,
        //  so the timing puts the stop past her tile: still hers.
        {.name = "open, seen, timed late", .leg = open, .walk_end = StepEnd::STOPPED, .walked = WalkedTiles{.min = 3, .max = 4}, .watch_end = StepEnd::DONE, .watches = 0, .seen = true},
        {.name = "to a wall, not seen", .leg = wall, .walk_end = StepEnd::DONE, .walked = std::nullopt, .watch_end = StepEnd::DONE, .watches = 1, .seen = false},
    };
    for (const TrainerCase& c : cases){
        const UnitTestResult result = run_trainer_case(maps, c);
        if (result.result != UnitTestResult::PASSED){
            return std::string(c.name) + ": " + result.message;
        }
    }
    return true;
}


//  On the grass a stop past Robin's column may be a wild battle's anywhere
//  on the leg, or hers on (3, 1), after which she stands on (3, 2): the
//  player is found again from all of them, then walks back to the leg's end.
UnitTestResult test_grass_in_sight(){
    const std::vector<MapData> maps{parse_map(TEST_ROBIN_JSON, "Robin")};
    RouteStep grass = walk(Direction::EAST, 4, robin(4, 1));
    grass.wild = true;
    RouteStep face = walk(Direction::NORTH, 0, grass.end);
    face.action = RouteAction::FACE;
    const std::vector<StopCandidate> candidates = stop_candidates(maps, robin(0, 1), grass, whole_leg(grass));
    const StopCandidate hers{.player = robin(3, 1), .trainer = TrainerStop{.object = 0, .at = robin(3, 2)}};
    if (candidates.size() != 6 || std::find(candidates.begin(), candidates.end(), hers) == candidates.end()){
        return std::format("Expected the leg's 5 tiles and hers, got {} candidates.", candidates.size());
    }
    const std::optional<Reanchor> anchor = plan_reanchor(maps, candidates, grass);
    if (!anchor || anchor->holds.empty()){
        return "Expected holds that find the player.";
    }
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = robin(0, 1), .label = "Route", .wall_check = WallCheck::SKIP};
    ScriptedRoute script;
    script.ends[0] = stop_part_way();
    if (script.run({grass, face}, &planning)){
        return "Failed: " + script.failure;
    }
    std::vector<RouteStep> expected{grass};
    expected.insert(expected.end(), anchor->holds.begin(), anchor->holds.end());
    expected.insert(expected.end(), anchor->back.begin(), anchor->back.end());
    expected.push_back(face);
    const UnitTestResult walked = expect_walked(expected, script.walked);
    if (walked.result != UnitTestResult::PASSED){
        return walked;
    }
    return expect_logged(script.logs, {"Robin (3, 1) (stopped by a trainer, now at (3, 2))"});
}

const OverworldRecoveryTestCase OVERWORLD_RECOVERY_CASES[] = {
    {
        .input = {.name = "stop, recover, resume", .body = test_stop_recover_resume},
        .expectation = {.description = "verifies full stop, recovery reanchor, and resume sequence"},
    },
    {
        .input = {.name = "no recovery", .body = test_no_recovery},
        .expectation = {.description = "handles clean runs where no recovery interruption occurs"},
    },
    {
        .input = {.name = "legs to a wall", .body = test_wall_legs},
        .expectation = {.description = "tests wall-stopping leg recovery"},
    },
    {
        .input = {.name = "a leg to a warp", .body = test_warp_leg},
        .expectation = {.description = "tests warp arrival leg recovery"},
    },
    {
        .input = {.name = "the route is logged", .body = test_route_logged},
        .expectation = {.description = "verifies route execution logging and breadcrumbs"},
    },
    {
        .input = {.name = "a leg to a wall ends at it", .body = test_wall_check},
        .expectation = {.description = "validates arrival position at wall terminator"},
    },
    {
        .input = {.name = "a hold that moved nothing", .body = test_unmoved_leg},
        .expectation = {.description = "a leg whose screen never moved is walked again, then fails; not taken as walked"},
    },
    {
        .input = {.name = "a hold that moved nothing from an unknown tile", .body = test_unmoved_hold_from_unknown_tile},
        .expectation = {.description = "a bump at the wall it may stand at is walked; no movement from short of it fails"},
    },
    {
        .input = {.name = "a leg crosses a trainer's sight", .body = test_trainer_crossing},
        .expectation = {.description = "recovers when traversing through trainer vision cones"},
    },
    {
        .input = {.name = "a trainer stops a leg", .body = test_trainer_stops_leg},
        .expectation = {.description = "handles forced stop and battle start from trainer challenge"},
    },
    {
        .input = {.name = "a stop on the grass in a trainer's sight", .body = test_grass_in_sight},
        .expectation = {.description = "handles wild encounter in trainer sight line"},
    },
};

} // namespace


void add_tests_OverworldRecovery(UnitTestDatabase& database){
    for (const auto& scenario : OVERWORLD_RECOVERY_CASES){
        database.add<Test_OverworldRecovery>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
