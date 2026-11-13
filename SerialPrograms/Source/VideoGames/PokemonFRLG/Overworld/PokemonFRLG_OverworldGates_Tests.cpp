/*  Overworld Gates Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's step rules under the gated planner, and the sim walking the routes
 *  that open a card-key gate and press a statue.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldGatedRoute.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::GateRules;
using Pokemon::GateState;

namespace{

struct GatesInput{
    std::string name;
    std::function<UnitTestResult()> body;
};
struct GatesExpectation{
};
struct GatesCase{
    GatesInput input;
    GatesExpectation expectation;
};

class Test_OverworldGates : public UnitTest{
public:
    explicit Test_OverworldGates(GatesCase test_case)
        : UnitTest("PokemonFRLG::OverworldGates - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    GatesCase m_case;
};

//  A corridor with a Silph Co. card-key door at (4, 1).
const std::vector<MapData>& door_maps(){
    static const std::vector<MapData> MAPS{parse_map(R"({
        "format": "pokesim-runtime-map/2", "name": "GateHall", "points": {}, "warps": [], "connections": [], "source": {"decomp_file": "", "revision": "test", "date": "2026-11-12"},
        "tiles": ["#########", "#...G...#", "#########"],
        "gates": {"door_1": {"tiles": [[4, 1]], "open_glyph": ".", "new_game": "closed", "opens_by": "card_key_face_a", "flag": "FLAG_TEST"}}
    })", "GateHall")};
    return MAPS;
}

//  The same corridor with a Mansion statue at (1, 0), above the start, and a
//  barrier at (4, 1) that is open while the switch is on.
const std::vector<MapData>& statue_maps(){
    static const std::vector<MapData> MAPS{parse_map(R"({
        "format": "pokesim-runtime-map/2", "name": "GateStatue", "points": {}, "warps": [], "connections": [], "source": {"decomp_file": "", "revision": "test", "date": "2026-11-12"},
        "tiles": ["#########", "#...G...#", "#########"],
        "gates": {"barrier_1": {"tiles": [[4, 1]], "open_glyph": ".", "new_game": "closed", "switch": "mansion", "open_when": "on"}},
        "switches": {"mansion": {"flag": "FLAG_TEST_SWITCH", "toggle": true, "floors": ["GateStatue"],
            "statues": [{"coord": [1, 0], "face_from": [1, 1], "facing": "north"}]}}
    })", "GateStatue")};
    return MAPS;
}

const MapPosition HALL_START{"GateHall", {1, 1}};
const MapPosition HALL_GOAL{"GateHall", {6, 1}};
const MapPosition STATUE_START{"GateStatue", {1, 1}};
const MapPosition STATUE_GOAL{"GateStatue", {6, 1}};

GateRules with_card_key(bool held){
    GateRules rules;
    rules.card_key_held = held;
    return rules;
}

int count_steps(const std::vector<RouteStep>& route, RouteAction action){
    return (int)std::count_if(route.begin(), route.end(), [&](const RouteStep& step){ return step.action == action; });
}

std::string describe(const std::vector<RouteStep>& route){
    std::string text;
    for (const RouteStep& step : route){
        text += "\n  " + to_string(step);
    }
    return text;
}


//  The game's step rules read a gate tile a map has not resolved as a wall.
UnitTestResult test_plain_route_stops_at_gate(){
    if (plan_overworld_route(door_maps(), HALL_START, RouteGoal{HALL_GOAL, std::nullopt})){
        return "The plain planner walked through a closed gate.";
    }
    return true;
}

UnitTestResult test_route_opens_gate(){
    const auto route = plan_overworld_route_gated(
        door_maps(), HALL_START, RouteGoal{HALL_GOAL, std::nullopt}, GateState(), with_card_key(true)
    );
    if (!route){
        return "No route to the far side of the door with the Card Key.";
    }
    if (count_steps(*route, RouteAction::OPEN_GATE) != 1 || !(route->back().end == HALL_GOAL)){
        return "Expected one open-gate step and the goal reached:" + describe(*route);
    }
    const auto opens = std::find_if(route->begin(), route->end(), [](const RouteStep& step){ return step.action == RouteAction::OPEN_GATE; });
    if (opens->direction != Direction::EAST || !opens->gate_change.opened.contains("GateHall/door_1")){
        return "The open-gate step does not face east and open GateHall/door_1:" + describe(*route);
    }
    if (!gates_after_route(GateState(), *route).opened.contains("GateHall/door_1")){
        return "gates_after_route() does not have the door open.";
    }
    return true;
}

UnitTestResult test_route_needs_card_key(){
    if (plan_overworld_route_gated(door_maps(), HALL_START, RouteGoal{HALL_GOAL, std::nullopt}, GateState(), with_card_key(false))){
        return "A route opened a card-key door without the Card Key.";
    }
    return true;
}

UnitTestResult test_route_through_open_gate(){
    GateState open;
    open.opened.insert(Pokemon::gate_key("GateHall", "door_1"));
    const auto route = plan_overworld_route_gated(
        door_maps(), HALL_START, RouteGoal{HALL_GOAL, std::nullopt}, open, with_card_key(false)
    );
    if (!route || count_steps(*route, RouteAction::OPEN_GATE) != 0){
        return "Expected a plain walk through the open door.";
    }
    return true;
}

UnitTestResult test_route_presses_statue(){
    const auto route = plan_overworld_route_gated(
        statue_maps(), STATUE_START, RouteGoal{STATUE_GOAL, std::nullopt}, GateState(), with_card_key(false)
    );
    if (!route){
        return "No route past the barrier by pressing the statue.";
    }
    if (count_steps(*route, RouteAction::PRESS_SWITCH) != 1 || !(route->back().end == STATUE_GOAL)){
        return "Expected one press-switch step and the goal reached:" + describe(*route);
    }
    if (!gates_after_route(GateState(), *route).switch_on("mansion")){
        return "gates_after_route() does not have the switch on.";
    }
    return true;
}


//  Walks `route` in the sim from `start`, with the Card Key as `card_key`.
struct SimRun{
    std::string failure;
    GateState gates;
    MapPosition position;
    std::vector<std::string> log;
};
SimRun walk_in_sim(const std::vector<MapData>& maps, const MapPosition& start, const std::vector<RouteStep>& route, bool card_key){
    SimScript script;
    script.card_key_held = card_key;
    OverworldSim sim(maps, start, Direction::EAST, script);
    OverworldSimWalker walker(sim, SimWalkerRules{});
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = start, .label = "Sim"};
    SimRun run;
    run.failure = walker.run(route, planning);
    run.gates = sim.state().gates;
    run.position = sim.state().position;
    run.log = sim.state().log;
    run.log.insert(run.log.end(), walker.log().begin(), walker.log().end());
    return run;
}

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n  " + line;
    }
    return text;
}

//  The leg to the sim's command: face the door, A; the door is open and the
//  rest of the route walks through it.
UnitTestResult test_sim_opens_gate(){
    const auto route = plan_overworld_route_gated(
        door_maps(), HALL_START, RouteGoal{HALL_GOAL, std::nullopt}, GateState(), with_card_key(true)
    );
    if (!route){
        return "No route.";
    }
    const SimRun run = walk_in_sim(door_maps(), HALL_START, *route, true);
    if (!run.failure.empty() || !(run.position == HALL_GOAL) || !run.gates.opened.contains("GateHall/door_1")){
        return std::format("Expected the door opened and the goal reached, ended at {}: \"{}\"", to_string(run.position), run.failure)
            + describe(*route) + joined(run.log);
    }
    return true;
}

UnitTestResult test_sim_presses_statue(){
    const auto route = plan_overworld_route_gated(
        statue_maps(), STATUE_START, RouteGoal{STATUE_GOAL, std::nullopt}, GateState(), with_card_key(false)
    );
    if (!route){
        return "No route.";
    }
    const SimRun run = walk_in_sim(statue_maps(), STATUE_START, *route, false);
    if (!run.failure.empty() || !(run.position == STATUE_GOAL) || !run.gates.switch_on("mansion")){
        return std::format("Expected the switch on and the goal reached, ended at {}: \"{}\"", to_string(run.position), run.failure)
            + describe(*route) + joined(run.log);
    }
    return true;
}

//  The sim, like the game, does not open a card-key door for a player
//  without the Card Key.
UnitTestResult test_sim_door_stays_shut_without_card_key(){
    const auto route = plan_overworld_route_gated(
        door_maps(), HALL_START, RouteGoal{HALL_GOAL, std::nullopt}, GateState(), with_card_key(true)
    );
    if (!route){
        return "No route.";
    }
    const SimRun run = walk_in_sim(door_maps(), HALL_START, *route, false);
    if (run.failure.empty() || run.gates.opened.contains("GateHall/door_1")){
        return "The door opened for a player without the Card Key." + joined(run.log);
    }
    return true;
}

const std::vector<GatesCase> GATES_CASES{
    {.input = {.name = "the plain planner: a closed gate is a wall", .body = test_plain_route_stops_at_gate}, .expectation = {}},
    {.input = {.name = "the route opens a card-key gate with the Card Key", .body = test_route_opens_gate}, .expectation = {}},
    {.input = {.name = "no route through a card-key gate without the Card Key", .body = test_route_needs_card_key}, .expectation = {}},
    {.input = {.name = "an open gate is walked through", .body = test_route_through_open_gate}, .expectation = {}},
    {.input = {.name = "the route presses a statue for a switch's barrier", .body = test_route_presses_statue}, .expectation = {}},
    {.input = {.name = "sim: face the door, A, walk through", .body = test_sim_opens_gate}, .expectation = {}},
    {.input = {.name = "sim: face the statue, A, YES, walk through", .body = test_sim_presses_statue}, .expectation = {}},
    {.input = {.name = "sim: the door stays shut without the Card Key", .body = test_sim_door_stays_shut_without_card_key}, .expectation = {}},
};

} // namespace


void add_tests_OverworldGates(UnitTestDatabase& database){
    for (const GatesCase& item : GATES_CASES){
        database.add<Test_OverworldGates>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
