/*  Overworld Re-anchor Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  stop_candidates() and plan_reanchor(): finding the player again at a
 *  wall after an open leg stopped part-way.
 *
 */

#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct OverworldReanchorInput{
    std::string name;
    UnitTestResult (*body)();
};

struct OverworldReanchorExpectation{
    std::string_view description;
};

struct OverworldReanchorTestCase{
    OverworldReanchorInput input;
    OverworldReanchorExpectation expectation;
};

class Test_OverworldReanchor : public UnitTest{
public:
    explicit Test_OverworldReanchor(OverworldReanchorTestCase scenario)
        : UnitTest("PokemonFRLG::OverworldReanchor - " + scenario.input.name)
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
    OverworldReanchorTestCase m_scenario;
};

std::string describe(const std::vector<RouteStep>& route){
    std::string text;
    for (const RouteStep& step : route){
        text += "\n  " + to_string(step);
    }
    return text;
}

std::string describe(const std::optional<Reanchor>& anchor){
    if (!anchor){
        return "none";
    }
    std::string text = to_string(anchor->found) + " after";
    for (const RouteStep& hold : anchor->holds){
        text += " " + to_string(hold);
    }
    return text + ", then" + describe(anchor->back);
}

//  How a walking leg may have stopped: anywhere (seen after the hold), and
//  at each estimate the timing can give.
std::vector<WalkedTiles> stops_of(const RouteStep& step){
    std::vector<WalkedTiles> stops{whole_leg(step)};
    for (int estimate = 0; estimate <= step.tiles; estimate++){
        stops.push_back(walked_near(step, estimate, std::chrono::milliseconds(267)));
    }
    return stops;
}

//  Plans from `start` to `goal`, then checks that the player can be found
//  again however an open leg through tall grass is stopped, and that the
//  way back reaches that leg's end.
UnitTestResult check_reanchors(const std::vector<MapData>& maps, const MapPosition& start, const RouteGoal& goal){
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, start, goal);
    if (!route){
        return "No route from " + to_string(start) + ".";
    }
    std::optional<MapPosition> at = start;
    for (const RouteStep& step : *route){
        if (step.action != RouteAction::WALK || !step.wild || step.to_wall){
            at = step.end;
            continue;
        }
        for (const WalkedTiles& walked : stops_of(step)){
            const std::vector<StopCandidate> candidates = stop_candidates(maps, at, step, walked);
            const std::optional<Reanchor> anchor = plan_reanchor(maps, candidates, step);
            if (!anchor){
                return std::format(
                    "Can't find the player again on {}, stopped {} to {} tiles in ({} candidates):{}",
                    to_string(step), walked.min, walked.max, candidates.size(), describe(*route)
                );
            }
            if (!(follow_overworld_route(maps, anchor->found, anchor->back) == step.end)){
                return "The way back from " + to_string(anchor->found) + " misses the end of " + to_string(step) + ".";
            }
        }
        at = step.end;
    }
    return true;
}

//  Console B, 2026-11-01: battles on Route 1 left the walker a tile or a
//  whole leg off its count, four times.
UnitTestResult test_viridian_mart_to_oaks_lab(){
    const std::vector<MapData> maps = overworld_maps();
    return check_reanchors(
        maps, {"ViridianCity_Mart", map_point(maps, "ViridianCity_Mart", "clerk_front")},
        {{"PalletTown_ProfessorOaksLab", {6, 4}}, std::nullopt}
    );
}
UnitTestResult test_pallet_to_viridian_mart(){
    const std::vector<MapData> maps = overworld_maps();
    return check_reanchors(
        maps, {"PalletTown", map_point(maps, "PalletTown", "oaks_lab_front")},
        {{"ViridianCity_Mart", map_point(maps, "ViridianCity_Mart", "clerk_front")}, std::nullopt}
    );
}
//  All grass, walled in by trees: where the walker was lost twice (console B,
//  2026-11-01, runs 16 and 17), and where the long dungeon walks start.
UnitTestResult test_viridian_forest(){
    const std::vector<MapData> maps = overworld_maps();
    return check_reanchors(
        maps, {"ViridianForest", {29, 61}},
        {{"Route2_ViridianForest_NorthEntrance", {7, 8}}, std::nullopt}
    );
}

//  A field with a wall at its east end and a row of ledges to the south.
const std::string TEST_FIELD_JSON = R"({
    "name": "Field", "source": {"decomp_file": "", "revision": "test", "date": "2026-10-31"},
    "tiles": ["########", "#......#", "#......#", "#vvvvvv#", "#......#"],
    "elevation": ["33333333", "33333333", "33333333", "33333333", "33333333"]
})";

//  Stopped part-way east, the player holds back west to the wall, the
//  shorter way, and walks back. Stopped a tile further from that wall, they
//  hold on east to the far wall: two more tiles held, one fewer to walk
//  back on an open leg, which a wild battle would stop again. Stopped
//  part-way south above the ledges, which would be hopped, they hold back
//  north.
UnitTestResult test_rules(){
    const std::vector<MapData> maps{parse_map(TEST_FIELD_JSON, "Field")};
    RouteStep east;
    east.direction = Direction::EAST;
    east.tiles = 2;
    east.end = {"Field", {3, 1}};
    std::optional<Reanchor> anchor = plan_reanchor(maps, stop_candidates(maps, MapPosition{"Field", {1, 1}}, east, whole_leg(east)), east);
    if (!anchor || anchor->holds.size() != 1 || anchor->holds[0].direction != Direction::WEST || !(anchor->found.point == TilePos{1, 1})){
        return "Expected a hold west to (1, 1), got " + describe(anchor);
    }
    if (anchor->back.size() != 1 || !(anchor->back[0].end == east.end)){
        return "Expected one leg back east to (3, 1), got " + describe(anchor);
    }
    east.tiles = 3;
    east.end = {"Field", {4, 1}};
    anchor = plan_reanchor(maps, stop_candidates(maps, MapPosition{"Field", {1, 1}}, east, whole_leg(east)), east);
    if (!anchor || anchor->holds.size() != 1 || anchor->holds[0].direction != Direction::EAST || !(anchor->found.point == TilePos{6, 1})){
        return "Expected a hold east to (6, 1), got " + describe(anchor);
    }
    RouteStep south;
    south.direction = Direction::SOUTH;
    south.tiles = 1;
    south.end = {"Field", {3, 2}};
    anchor = plan_reanchor(maps, stop_candidates(maps, std::nullopt, south, whole_leg(south)), south);
    if (!anchor || anchor->holds.size() != 1 || anchor->holds[0].direction != Direction::NORTH || !(anchor->found.point == TilePos{3, 1})){
        return "Expected a hold north back to (3, 1), got " + describe(anchor);
    }
    return true;
}

//  The timing narrows a stop to a tile either side of its estimate walking,
//  two running, within the leg.
UnitTestResult test_timing(){
    RouteStep leg;
    leg.direction = Direction::EAST;
    leg.tiles = 10;
    struct Case{
        int estimate;
        int tile_ms;
        int min;
        int max;
    };
    const Case cases[] = {
        {.estimate = 5, .tile_ms = 267, .min = 4, .max = 6},
        {.estimate = 0, .tile_ms = 267, .min = 0, .max = 1},
        {.estimate = 10, .tile_ms = 267, .min = 9, .max = 10},
        {.estimate = 5, .tile_ms = 133, .min = 3, .max = 7},
        {.estimate = 1, .tile_ms = 133, .min = 0, .max = 3},
    };
    for (const Case& c : cases){
        const WalkedTiles walked = walked_near(leg, c.estimate, std::chrono::milliseconds(c.tile_ms));
        if (walked.min != c.min || walked.max != c.max){
            return std::format(
                "{} tiles in at {}ms a tile: expected {} to {}, got {} to {}.",
                c.estimate, c.tile_ms, c.min, c.max, walked.min, walked.max
            );
        }
    }
    return true;
}

//  A field with someone standing in the middle of its north row.
const std::string TEST_PERSON_JSON = R"({
    "name": "Person", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-02"},
    "tiles": ["#######", "#.....#", "#.....#", "#######"],
    "objects": [{"name": "youngster", "kind": "npc", "x": 3, "y": 1}]
})";

//  Stopped going east along the south row: holding north would meet the
//  person from one of the tiles, so the player holds along the row. On the
//  north row, past the person, they don't hold west into them.
UnitTestResult test_person_in_the_way(){
    const std::vector<MapData> maps{parse_map(TEST_PERSON_JSON, "Person")};
    RouteStep east;
    east.direction = Direction::EAST;
    east.tiles = 4;
    east.end = {"Person", {5, 2}};
    const std::optional<Reanchor> anchor = plan_reanchor(maps, stop_candidates(maps, MapPosition{"Person", {1, 2}}, east, whole_leg(east)), east);
    if (!anchor || anchor->holds.size() != 1 || anchor->holds[0].direction == Direction::NORTH){
        return "Expected one hold along the row, clear of the person, got " + describe(anchor);
    }
    RouteStep north_row = east;
    north_row.tiles = 1;
    north_row.end = {"Person", {5, 1}};
    const std::vector<StopCandidate> beside{{.player = {"Person", {4, 1}}, .trainer = std::nullopt}, {.player = {"Person", {5, 1}}, .trainer = std::nullopt}};
    const std::optional<Reanchor> blocked = plan_reanchor(maps, beside, north_row);
    if (blocked && blocked->holds[0].direction == Direction::WEST){
        return "Expected no hold west into the person, got " + describe(blocked);
    }
    return true;
}


//  A walk of `route` from `start` on the sim, with `script`'s faults: the
//  walker's failure (empty if none), the sim's state, and both logs.
struct SimWalk{
    std::string failure;
    SimState state;
    std::string log;
};
SimWalk walk_on_sim(const MapPosition& start, Direction facing, const std::vector<RouteStep>& route, const SimScript& script){
    static const std::vector<MapData> MAPS = overworld_maps();
    OverworldSim sim(MAPS, start, facing, script);
    OverworldSimWalker walker(sim);
    const RoutePlanning planning{.maps = &MAPS, .costs = default_route_costs(), .start = start, .label = "Sim"};
    SimWalk out{.failure = walker.run(route, planning), .state = sim.state(), .log = ""};
    for (const std::vector<std::string>& lines : {walker.log(), sim.state().log}){
        for (const std::string& line : lines){
            out.log += "\n  " + line;
        }
    }
    return out;
}

RouteStep sim_leg(Direction direction, int tiles, MapPosition end, bool to_wall){
    RouteStep step;
    step.direction = direction;
    step.tiles = tiles;
    step.to_wall = to_wall;
    step.facing_after = direction;
    step.end = std::move(end);
    return step;
}

//  12.03 pallet_shore (console B, 2026-11-13 18:29): after "east 4 (open)"
//  the walker found the player at the east wall, and the way back, "west 2
//  (open)" to (10, 10), went a tile long, onto (9, 10) above the Pallet
//  Town sign (9, 11). "south 6 (to wall)" read the sign at every hold, 6
//  times. The second read now stops the leg, and the sign places the
//  player: of the tiles near the leg's start, only from (9, 10) does a hold
//  south stop below a sign.
UnitTestResult test_pallet_sign_after_long_leg(){
    const MapPosition goal{"PalletTown", {10, 16}};
    const std::vector<RouteStep> route{
        sim_leg(Direction::SOUTH, 2, {"PalletTown", {6, 10}}, true),
        sim_leg(Direction::EAST, 4, {"PalletTown", {10, 10}}, false),
        sim_leg(Direction::SOUTH, 6, goal, true),
    };
    SimScript script;
    script.long_holds[4] = 1;
    const SimWalk run = walk_on_sim({"PalletTown", {6, 8}}, Direction::SOUTH, route, script);
    if (!run.failure.empty() || !(run.state.position == goal) || run.state.signs_read > 2){
        return std::format("Expected the player placed by the sign and the route walked, ended on {}: \"{}\"{}", to_string(run.state.position), run.failure, run.log);
    }
    return true;
}

//  13.01 mansion_holes (console B, 2026-11-13 19:20), Pokemon Mansion 2F: a
//  wild battle stopped an open leg west; the hold that finds the player,
//  west to the wall, saw a second battle only after it, on its last step,
//  and was held again. At the wall, that hold moved nothing, and the walk
//  failed as if the player never moved. It is now a bump at its wall.
UnitTestResult test_mansion_battle_then_hold_again(){
    const MapPosition goal{"PokemonMansion_2F", {4, 5}};
    std::vector<RouteStep> route{
        sim_leg(Direction::WEST, 7, {"PokemonMansion_2F", {4, 6}}, false),
        sim_leg(Direction::NORTH, 1, goal, true),
    };
    for (RouteStep& step : route){
        step.wild = true;
    }
    SimScript script;
    script.wild_floor_maps = {"PokemonMansion_2F"};
    script.encounters = {{.grass_step = 3, .seen_late = false}, {.grass_step = 9, .seen_late = true}};
    const SimWalk run = walk_on_sim({"PokemonMansion_2F", {11, 6}}, Direction::WEST, route, script);
    if (!run.failure.empty() || !(run.state.position == goal)){
        return std::format("Expected the hold held again taken as at its wall, ended on {}: \"{}\"{}", to_string(run.state.position), run.failure, run.log);
    }
    return true;
}

//  The walk from the statue's corner of Pokemon Mansion B1F, (25, 31), to
//  the switch, (24, 30): "west 1 (open)", then "north 1 (to wall)".
std::vector<RouteStep> mansion_b1f_to_switch(){
    std::vector<RouteStep> route{
        sim_leg(Direction::WEST, 1, {"PokemonMansion_B1F", {24, 31}}, false),
        sim_leg(Direction::NORTH, 1, {"PokemonMansion_B1F", {24, 30}}, true),
    };
    for (RouteStep& step : route){
        step.wild = true;
    }
    return route;
}

//  13.03 to_elixir (console B, 2026-11-14 04:43), Pokemon Mansion B1F:
//  "west 1 (open)" from (25, 31) scrolled the screen (39.2) and ended under
//  the statue, but the open-leg check took it as maybe 0 tiles walked and
//  found the player at walls: "east 1 (to wall)" back to (25, 31), then the
//  same leg again. A hold that scrolled walked at least a tile, so a
//  one-tile leg ended on its tile, and nothing walks back over it.
UnitTestResult test_mansion_one_tile_leg_scrolled(){
    const MapPosition start{"PokemonMansion_B1F", {25, 31}};
    const SimWalk run = walk_on_sim(start, Direction::SOUTH, mansion_b1f_to_switch(), SimScript{});
    const MapPosition goal{"PokemonMansion_B1F", {24, 30}};
    if (!run.failure.empty() || !(run.state.position == goal) || run.log.find("position check") != std::string::npos){
        return std::format("Expected the one-tile leg trusted and the switch reached, ended on {}: \"{}\"{}", to_string(run.state.position), run.failure, run.log);
    }
    return true;
}

//  As above, the second try: a wild battle seen after the hold of "west 1
//  (open)" left the tile unknown, (25, 31) or (24, 31), and the hold that
//  finds the player, "east 1 (to wall)", failed at its turn tap: the
//  facing never read there, the legs hidden by the partition's base drawn
//  over them, and a turn whose facing doesn't read is taken only on a known
//  tile. A hold to a wall turns the player itself if the tap was missed,
//  and ends at the wall from either tile, so it goes on.
UnitTestResult test_mansion_unread_turn_before_wall_hold(){
    const MapPosition start{"PokemonMansion_B1F", {25, 31}};
    SimScript script;
    script.wild_floor_maps = {"PokemonMansion_B1F"};
    script.encounters = {{.grass_step = 1, .seen_late = true}};
    script.hidden_facing_reads = SIZE_MAX;
    const SimWalk run = walk_on_sim(start, Direction::SOUTH, mansion_b1f_to_switch(), script);
    if (run.log.find("plan from an unknown tile") == std::string::npos){
        return "Expected the battle to leave the tile unknown, as on the console:" + run.log;
    }
    const MapPosition goal{"PokemonMansion_B1F", {24, 30}};
    if (!run.failure.empty() || !(run.state.position == goal)){
        return std::format("Expected the turn before the hold to the wall taken, and the switch reached, ended on {}: \"{}\"{}", to_string(run.state.position), run.failure, run.log);
    }
    return true;
}

const OverworldReanchorTestCase OVERWORLD_REANCHOR_CASES[] = {
    {
        .input = {.name = "rules", .body = test_rules},
        .expectation = {.description = "tests reanchor candidate generation and rules"},
    },
    {
        .input = {.name = "timing", .body = test_timing},
        .expectation = {.description = "verifies reanchor hold durations and timing"},
    },
    {
        .input = {.name = "a person in the way", .body = test_person_in_the_way},
        .expectation = {.description = "avoids planning reanchor into dynamic map obstacle"},
    },
    {
        .input = {.name = "sim: Pallet Town, a long leg, then the sign", .body = test_pallet_sign_after_long_leg},
        .expectation = {.description = "a sign that speaks at every hold places the player before it"},
    },
    {
        .input = {.name = "sim: Pokemon Mansion 2F, a battle, then a hold held again at its wall", .body = test_mansion_battle_then_hold_again},
        .expectation = {.description = "a hold to a wall held again that moves nothing is a bump at the wall"},
    },
    {
        .input = {.name = "sim: Pokemon Mansion B1F, a one-tile open leg that scrolled", .body = test_mansion_one_tile_leg_scrolled},
        .expectation = {.description = "a one-tile open leg whose hold scrolled ends on its tile, with no walls to find"},
    },
    {
        .input = {.name = "sim: Pokemon Mansion B1F, a turn not read before a hold to a wall", .body = test_mansion_unread_turn_before_wall_hold},
        .expectation = {.description = "on an unknown tile, an unread turn before a hold to a wall is taken as turned"},
    },
    {
        .input = {.name = "Viridian Mart to Oak's Lab", .body = test_viridian_mart_to_oaks_lab},
        .expectation = {.description = "reanchors along route from Viridian Mart to Oak's Lab"},
    },
    {
        .input = {.name = "Pallet Town to Viridian Mart", .body = test_pallet_to_viridian_mart},
        .expectation = {.description = "reanchors along route from Pallet Town to Viridian Mart"},
    },
    {
        .input = {.name = "Viridian Forest", .body = test_viridian_forest},
        .expectation = {.description = "reanchors along route through Viridian Forest"},
    },
};

} // namespace


void add_tests_OverworldReanchor(UnitTestDatabase& database){
    for (const auto& scenario : OVERWORLD_REANCHOR_CASES){
        database.add<Test_OverworldReanchor>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
