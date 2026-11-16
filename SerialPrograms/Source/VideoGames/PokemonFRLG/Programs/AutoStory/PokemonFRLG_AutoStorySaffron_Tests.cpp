/*  AutoStory Saffron Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 15.01 (StoryData/frlg-15.01.plan.json) on the overworld sim: from
 *  Celadon's nurse along Route 7 into its east gate, onto the guard's
 *  trigger, and through Saffron City to its nurse.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows15_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySaffronInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStorySaffronExpectation{
    std::string_view description;
};

struct AutoStorySaffronCase{
    AutoStorySaffronInput input;
    AutoStorySaffronExpectation expectation;
};

class Test_AutoStorySaffron : public UnitTest{
public:
    explicit Test_AutoStorySaffron(AutoStorySaffronCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStorySaffronCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  15.01 on the sim from Celadon's nurse: Route 7, the gate's top trigger
//  (6, 4), where the TEA scene leaves the player, out of the gate onto
//  Saffron City and the Fly to below its Center's door; the way into the
//  Center, which has no map, is the console's. No trainer is met.
UnitTestResult test_route7_to_saffron(){
    const StoryCheckpoint& row = checkpoint("15.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    const MapPosition trigger{"Route7_EastEntrance", {6, 4}};
    bool on_trigger = false;
    for (const StoryStep& step : row.steps){
        if (step.action == CENTER_ENTER_VARIANT){
            break;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return std::format("15.01: ended on {}; {}", to_string(sim.state().position), failure);
        }
        on_trigger = on_trigger || sim.state().position == trigger;
    }
    if (!on_trigger){
        return std::format("15.01: never stood on the guard's trigger {}.", to_string(trigger));
    }
    const MapPosition below_center{"SaffronCity", {24, 39}};
    if (!(sim.state().position == below_center)){
        return std::format("15.01: ended on {}, not below Saffron's Center door {}.", to_string(sim.state().position), to_string(below_center));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "15.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  15.01's walk to the gate as console B ran it (2026-11-14 20:23 and
//  20:27, twice alike): running, and the game not holding the player up on
//  the crossing east onto Route 7, so the open leg east ran a column long,
//  onto column 8. Dead-reckoned, "south 8" down that column hopped the
//  ledge on row 8, which took two of its tiles, and ended on row 9; "east 9
//  (warp)" then held into the gate's wall a row above its door. Held to
//  walls, the walk ends at the door from column 7 or 8 alike.
UnitTestResult test_route7_gate_running_crossing_not_held_up(){
    const StoryCheckpoint& row = checkpoint("15.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    SimScript script;
    script.timing.tile = MovementMechanics::RUN_TILE_TIME;
    script.connections_stall = false;
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing, script);
    SimWalkerRules rules;
    rules.speed = MoveSpeed::RUN;
    for (const StoryStep& step : row.steps){
        if (step.name == "tea_trigger"){
            break;
        }
        const std::optional<std::vector<RouteStep>> route = walked_route(at, step, story_maps(), sim.state().gates);
        if (!route || route->empty()){
            return std::format("15.01 {}: no route from {}.", step.name, to_string(MapPosition{at.map, at.tile}));
        }
        const RoutePlanning planning{
            .maps = &story_maps(), .gates = sim.state().gates, .costs = story_route_costs(),
            .start = MapPosition{at.map, at.tile}, .label = step.name
        };
        if (const std::string failure = OverworldSimWalker(sim, rules).run(*route, planning); !failure.empty()){
            return std::format("15.01 {}: ended on {}; {}", step.name, to_string(sim.state().position), failure);
        }
        at = StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after};
    }
    const MapPosition west_of_trigger{"Route7_EastEntrance", {5, 4}};
    if (!(sim.state().position == west_of_trigger)){
        return std::format("15.01: ended on {}, not west of the guard's trigger, {}.", to_string(sim.state().position), to_string(west_of_trigger));
    }
    return true;
}

//  15.01 west_of_tea_guard as console B ran it (2026-11-14 22:27, run 3):
//  from the gate's wall (15, 11) facing north, "north 1" onto the door's
//  tile (15, 10), an exit mat taken east, then "east 1 (warp)". The turn
//  tap east took the warp, its fade passed while the turn was read, and the
//  hold and the two steps more walked on inside the gate: "no fade in and
//  out after walking onto the warp".
UnitTestResult test_route7_gate_door_turn_tap(){
    const StoryCheckpoint& row = checkpoint("15.01");
    StoryPose at{"Route7", {15, 11}, Direction::NORTH};
    SimScript script;
    script.timing.tile = MovementMechanics::RUN_TILE_TIME;
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing, script);
    SimWalkerRules rules;
    rules.speed = MoveSpeed::RUN;
    for (const StoryStep& step : row.steps){
        if (step.name != "west_of_tea_guard"){
            continue;
        }
        const std::optional<std::vector<RouteStep>> route = walked_route(at, step, story_maps(), sim.state().gates);
        if (!route || route->empty()){
            return std::format("15.01 {}: no route from {}.", step.name, to_string(MapPosition{at.map, at.tile}));
        }
        const RoutePlanning planning{
            .maps = &story_maps(), .gates = sim.state().gates, .costs = story_route_costs(),
            .start = MapPosition{at.map, at.tile}, .label = step.name
        };
        if (const std::string failure = OverworldSimWalker(sim, rules).run(*route, planning); !failure.empty()){
            return std::format("15.01 {}: ended on {}; {}", step.name, to_string(sim.state().position), failure);
        }
    }
    const MapPosition west_of_trigger{"Route7_EastEntrance", {5, 4}};
    if (!(sim.state().position == west_of_trigger)){
        return std::format("15.01: ended on {}, not west of the guard's trigger, {}.", to_string(sim.state().position), to_string(west_of_trigger));
    }
    return true;
}

const AutoStorySaffronCase AUTO_STORY_SAFFRON_CASES[] = {
    {
        .input = {.name = "15.01 Route 7's gate with the TEA to Saffron's Center", .body = test_route7_to_saffron},
        .expectation = {.description = "onto the guard's trigger, out of the gate, the Fly to below Saffron's Center door, nobody met"},
    },
    {
        .input = {.name = "15.01 running, the crossing onto Route 7 not held up", .body = test_route7_gate_running_crossing_not_held_up},
        .expectation = {.description = "a column long after the crossing, the walk still ends at the gate's guard"},
    },
    {
        .input = {.name = "15.01 west_of_tea_guard from the gate's wall, the door an exit mat", .body = test_route7_gate_door_turn_tap},
        .expectation = {.description = "the press east off the door's tile takes the warp with no turn tap first; ends west of the guard's trigger"},
    },
};

} // namespace


void add_tests_AutoStorySaffron(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SAFFRON_CASES){
        database.add<Test_AutoStorySaffron>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
