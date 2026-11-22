/*  Overworld Wander Recovery Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_MapData.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldRouteCosts.h"
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldWanderRecovery_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

RouteStep walk_step(Direction direction, int tiles, MapPosition end){
    RouteStep step;
    step.action = RouteAction::WALK;
    step.direction = direction;
    step.tiles = tiles;
    step.end = std::move(end);
    step.facing_after = direction;
    return step;
}

struct WanderScript{
    std::map<size_t, StepOutcome> ends;
    std::vector<bool> wall_tap_moves;
    std::vector<RouteStep> walked;
    std::vector<Milliseconds> waits;
    std::string failure;
    size_t wall_checks = 0;

    bool run(const std::vector<RouteStep>& route, const RoutePlanning* planning){
        RouteCalls calls;
        calls.walk = [&](const RouteStep& step, std::optional<Direction>){
            const size_t index = walked.size();
            walked.push_back(step);
            const auto found = ends.find(index);
            return found != ends.end() ? found->second : StepOutcome{};
        };
        calls.moved_past_wall = [&](const RouteStep&){
            const size_t index = wall_checks++;
            return index < wall_tap_moves.size() && wall_tap_moves[index];
        };
        calls.wait = [&](Milliseconds duration){ waits.push_back(duration); };
        calls.log = [](const std::string&){};
        calls.fail = [&](const std::string& message){ failure = message; };
        run_route(route, std::nullopt, planning, calls);
        return !failure.empty();
    }
};

std::vector<MapData> make_cerulean_wander_maps(){
    std::vector<std::string> rows(16, std::string(26, '.'));
    rows[0] = std::string(26, '#');
    rows[15] = std::string(26, '#');
    for (int y = 0; y < 16; y++){
        rows[y][0] = '#';
        rows[y][25] = '#';
    }
    for (int y = 1; y <= 11; y++){
        for (int x = 12; x <= 21; x++){
            rows[y][x] = '#';
        }
        rows[y][24] = '#';
    }
    rows[13] = std::string(26, '#');
    std::string tiles_json = "[";
    std::string elev_json = "[";
    for (size_t y = 0; y < rows.size(); y++){
        tiles_json += std::format("{}\"{}\"", y > 0 ? "," : "", rows[y]);
        elev_json += std::format("{}\"{}\"", y > 0 ? "," : "", std::string(26, '3'));
    }
    tiles_json += "]";
    elev_json += "]";
    const std::string json = std::format(R"({{
        "name": "CeruleanCity", "source": {{"decomp_file": "", "revision": "test", "date": "2026-11-20"}},
        "tiles": {}, "elevation": {},
        "objects": [
            {{"name": "lass", "kind": "npc", "x": 18, "y": 12, "wander": {{"x0": 18, "y0": 12, "x1": 19, "y1": 12}}}}
        ]
    }})", tiles_json, elev_json);
    return {parse_map(json, "CeruleanCity")};
}

UnitTestResult test_wanderer_unmoved_wait_and_retry(){
    const std::string json = R"({
        "name": "Corridor", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-19"},
        "tiles": ["#######", "#.....#", "#.....#", "#######"],
        "elevation": ["3333333", "3333333", "3333333", "3333333"],
        "objects": [
            {"name": "youngster", "kind": "npc", "x": 3, "y": 1, "wander": {"x0": 3, "y0": 1, "x1": 3, "y1": 2}}
        ]
    })";
    const std::vector<MapData> maps{parse_map(json, "Corridor")};
    const MapPosition start{"Corridor", {2, 1}};
    RouteStep leg = walk_step(Direction::EAST, 3, {"Corridor", {5, 1}});
    leg.to_wall = true;
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = start, .label = "Route"};
    const StepOutcome unmoved{.end = StepEnd::STOPPED, .walked = WalkedTiles{.min = 0, .max = 0}, .movement = HoldMovement::NONE};

    WanderScript retries;
    retries.ends[0] = unmoved;
    retries.ends[1] = unmoved;
    if (retries.run({leg}, &planning) || retries.walked.size() != 3 || retries.waits.size() != 2){
        return std::format("Expected 3 walks and 2 waits, got {} walks, {} waits: {}",
            retries.walked.size(), retries.waits.size(), retries.failure);
    }

    WanderScript exhausted;
    for (size_t i = 0; i < MAX_WANDER_WAITS + 3; i++){
        exhausted.ends[i] = unmoved;
    }
    if (!exhausted.run({leg}, &planning) || exhausted.waits.size() != MAX_WANDER_WAITS){
        return std::format("Expected failure after {} waits, got {} waits (failed={})",
            MAX_WANDER_WAITS, exhausted.waits.size(), !exhausted.failure.empty());
    }
    return true;
}

UnitTestResult test_wanderer_wall_rehold(){
    const std::string json = R"({
        "name": "Corridor", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-19"},
        "tiles": ["#######", "#.....#", "#######"],
        "elevation": ["3333333", "3333333", "3333333"],
        "objects": [
            {"name": "youngster", "kind": "npc", "x": 3, "y": 1, "wander": {"x0": 3, "y0": 1, "x1": 4, "y1": 1}}
        ]
    })";
    const std::vector<MapData> maps{parse_map(json, "Corridor")};
    const MapPosition start{"Corridor", {1, 1}};
    RouteStep leg = walk_step(Direction::EAST, 4, {"Corridor", {5, 1}});
    leg.to_wall = true;
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = start, .label = "Route", .wall_check = WallCheck::TAP};

    WanderScript script;
    script.wall_tap_moves = {true, false};
    if (script.run({leg}, &planning) || script.walked.size() != 2 || script.waits.size() != 1){
        return std::format("Expected 2 walks and 1 wait after moved_past_wall, got {} walks, {} waits: {}",
            script.walked.size(), script.waits.size(), script.failure);
    }
    return true;
}

UnitTestResult test_open_leg_wanderer_mid_hold_block(){
    const std::vector<MapData> maps = make_cerulean_wander_maps();
    const MapPosition start{"CeruleanCity", {10, 12}};
    const MapPosition goal{"CeruleanCity", {23, 1}};
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = start, .label = "Route"};
    SimScript block_script;
    block_script.wanderer_blocks.push_back(SimWandererBlock{.map = "CeruleanCity", .tile = {18, 12}, .clear_after_waits = 1});

    //  1. Un-tapped hold bug repro: east 13 is bumped at (18, 12), missing goal.
    RouteStep east = walk_step(Direction::EAST, 13, {"CeruleanCity", {23, 12}});
    RouteStep north = walk_step(Direction::NORTH, 11, goal);
    north.to_wall = true;
    OverworldSim untap_sim(maps, start, Direction::EAST, block_script);
    OverworldSimWalker(untap_sim, SimWalkerRules{}).run({east, north}, planning, Direction::EAST);
    if (untap_sim.state().position == goal){
        return "Expected untap-marked hold to be bumped at (18, 12) and miss goal";
    }

    //  2. Planned route marks east 13 tapped and recovers mid-leg via wait + resume.
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, start, RouteGoal{goal, std::nullopt});
    if (!route || route->empty() || !(*route)[0].tapped){
        return "Expected planned route with tapped first leg crossing wander box";
    }
    OverworldSim sim(maps, start, Direction::EAST, block_script);
    const std::string err = OverworldSimWalker(sim, SimWalkerRules{}).run(*route, planning, Direction::EAST);
    if (!err.empty() || !(sim.state().position == goal) || sim.state().time < WANDER_WAIT_DURATION){
        return std::format("Expected goal {} after wait, got {} ({}ms, err=\"{}\")",
            to_string(goal), to_string(sim.state().position), sim.state().time.count(), err);
    }
    return true;
}

UnitTestResult test_reanchor_avoids_sight_via_wanderer(){
    const std::string json = R"({
        "name": "ReanchorWander", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-21"},
        "tiles": [
            "#######",
            "#.....#",
            "#.....#",
            "#.....#",
            "#.....#",
            "#######"
        ],
        "elevation": [
            "3333333",
            "3333333",
            "3333333",
            "3333333",
            "3333333",
            "3333333"
        ],
        "objects": [
            {"name": "trainer", "kind": "npc", "x": 5, "y": 1, "facing": "west", "trainer": {"sight": 3, "looks": ["west"]}},
            {"name": "wanderer", "kind": "npc", "x": 3, "y": 3, "wander": {"x0": 2, "y0": 3, "x1": 4, "y1": 3}}
        ]
    })";
    const std::vector<MapData> maps{parse_map(json, "ReanchorWander")};
    RouteStep step;
    step.direction = Direction::SOUTH;
    step.tiles = 2;
    step.facing_after = Direction::SOUTH;
    step.end = {"ReanchorWander", {3, 4}};
    const std::vector<StopCandidate> candidates{
        StopCandidate{.player = {"ReanchorWander", {3, 2}}, .trainer = std::nullopt},
        StopCandidate{.player = {"ReanchorWander", {3, 3}}, .trainer = std::nullopt},
    };
    const std::optional<Reanchor> anchor = plan_reanchor(maps, candidates, step, default_route_costs());
    if (!anchor || anchor->in_sight){
        return "Expected reanchor fallback across wanderer to avoid trainer sight line";
    }
    if (anchor->holds.empty() || anchor->holds[0].direction == Direction::NORTH){
        return "Expected non-NORTH reanchor hold avoiding trainer sight line";
    }
    return true;
}

struct WanderRecoveryTestCase{
    std::string name = {};
    struct Input{
        std::function<UnitTestResult()> body = {};
    };
    struct Expectation{
        std::string_view description = {};
    };
    Input input = {};
    Expectation expectation = {};
};

const WanderRecoveryTestCase WANDER_CASES[] = {
    {
        .name = "wanderer unmoved wait and retry",
        .input = {.body = test_wanderer_unmoved_wait_and_retry},
        .expectation = {.description = "waits and retries up to MAX_WANDER_WAITS when blocked by a wandering NPC"},
    },
    {
        .name = "wanderer wall re-hold",
        .input = {.body = test_wanderer_wall_rehold},
        .expectation = {.description = "waits and re-holds a to_wall leg touching a wander box when moved_past_wall detects movement"},
    },
    {
        .name = "open leg wanderer mid-hold block",
        .input = {.body = test_open_leg_wanderer_mid_hold_block},
        .expectation = {.description = "taps open legs crossing a wander box and waits/resumes when blocked mid-leg by a wandering NPC"},
    },
    {
        .name = "reanchor avoids sight via wanderer",
        .input = {.body = test_reanchor_avoids_sight_via_wanderer},
        .expectation = {.description = "falls back to crossing a wander box when the only non-wanderer reanchor crosses trainer sight"},
    },
};

class Test_OverworldWanderRecovery : public UnitTest{
public:
    explicit Test_OverworldWanderRecovery(const WanderRecoveryTestCase& scenario)
        : UnitTest(std::format("PokemonFRLG::OverworldRecovery - {}", scenario.name))
        , m_case(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    const WanderRecoveryTestCase& m_case;
};

} // namespace


void add_tests_OverworldWanderRecovery(UnitTestDatabase& database){
    for (const WanderRecoveryTestCase& scenario : WANDER_CASES){
        database.add<Test_OverworldWanderRecovery>(scenario);
    }
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
