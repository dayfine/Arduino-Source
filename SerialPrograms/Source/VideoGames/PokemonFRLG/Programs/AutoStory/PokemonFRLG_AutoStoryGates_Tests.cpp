/*  AutoStory Gates Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A ROUTE step on the row sims with gates: StoryMapsSim (the whole story
 *  on maps) and OverworldStepSim (a row's steps on the OverworldSim) stop at
 *  a closed Silph Co. card-key door, open it for a player with the Card Key
 *  and keep the gate state, and press a Pokemon Mansion statue for the
 *  barrier its switch opens.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryGates_Tests.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRowSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

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

class Test_AutoStoryGates : public UnitTest{
public:
    explicit Test_AutoStoryGates(GatesCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryGates - " + test_case.input.name)
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

//  A walk across a gate on the real maps: Silph Co. 5F's card-key door, and
//  the Pokemon Mansion 1F's barrier behind its statue's switch.
struct Scene{
    const char* map;
    TilePos start;
    TilePos goal;
};
constexpr Scene DOOR{"SilphCo_5F", {10, 9}, {5, 9}};
constexpr Scene STATUE{"PokemonMansion_1F", {5, 8}, {19, 13}};

const std::vector<MapData>& scene_maps(){
    static const std::vector<MapData> MAPS = overworld_maps();
    return MAPS;
}

StoryStep route_step(const std::string& map, TilePos goal){
    StoryStep step;
    step.kind = StoryStepKind::ROUTE;
    step.name = "cross";
    step.args["target"] = Pokemon::GridPose{map, {goal}, std::nullopt};
    return step;
}

Pokemon::RunState run_state(const Scene& scene, bool card_key){
    Pokemon::RunState state;
    state.pose = to_pose(StoryPose{scene.map, scene.start, Direction::EAST});
    if (card_key){
        state.flags.insert("card_key_held");
    }
    return state;
}

//  The step on StoryMapsSim over `maps`, from the start of the corridor.
Pokemon::Outcome on_maps(const Scene& scene, bool card_key, Pokemon::RunState& state){
    StorySimKnobs knobs;
    knobs.maps = &scene_maps();
    StoryMapsSim sim(knobs);
    state = run_state(scene, card_key);
    return sim.run(route_step(scene.map, scene.goal), state);
}


//  A closed door stops a player without the Card Key: the story's sim does
//  not walk through gates the run has not opened.
UnitTestResult test_maps_sim_door_blocks(){
    Pokemon::RunState state;
    const Pokemon::Outcome outcome = on_maps(DOOR, false, state);
    if (outcome.succeeded()){
        return "The row sim walked through a closed card-key door without the Card Key.";
    }
    return true;
}

UnitTestResult test_maps_sim_door_opens(){
    Pokemon::RunState state;
    const Pokemon::Outcome outcome = on_maps(DOOR, true, state);
    if (!outcome.succeeded()){
        return "The row sim did not open the door for a player with the Card Key: " + Pokemon::to_string(outcome);
    }
    if (!state.gates.opened.contains("SilphCo_5F/door_1")){
        return "The run's gates do not have the door open after the step.";
    }
    return true;
}

UnitTestResult test_maps_sim_statue_switch(){
    Pokemon::RunState state;
    const Pokemon::Outcome outcome = on_maps(STATUE, false, state);
    if (!outcome.succeeded()){
        return "The row sim did not press the statue for the barrier: " + Pokemon::to_string(outcome);
    }
    if (!state.gates.switch_on("mansion")){
        return "The run's gates do not have the switch on after the step.";
    }
    return true;
}

struct GateStateOut{
    Pokemon::GateState gates;
    MapPosition position;
};

//  The step on OverworldStepSim and its OverworldSim, with the Card Key as `card_key`.
std::string on_overworld_sim(const Scene& scene, bool card_key, GateStateOut& out){
    SimScript script;
    script.card_key_held = card_key;
    OverworldSim sim(scene_maps(), MapPosition{scene.map, scene.start}, Direction::EAST, script);
    StoryPose at{scene.map, scene.start, Direction::EAST};
    const std::string failure = row_step_on_sim(sim, at, route_step(scene.map, scene.goal), "test");
    out.gates = sim.state().gates;
    out.position = sim.state().position;
    return failure;
}

UnitTestResult test_step_sim_door_blocks(){
    GateStateOut out;
    if (on_overworld_sim(DOOR, false, out).empty()){
        return "The step walked through a closed card-key door without the Card Key.";
    }
    return true;
}

UnitTestResult test_step_sim_door_opens(){
    GateStateOut out;
    const std::string failure = on_overworld_sim(DOOR, true, out);
    if (!failure.empty()){
        return "The step did not open the door for a player with the Card Key: " + failure;
    }
    if (!out.gates.opened.contains("SilphCo_5F/door_1") || !(out.position == MapPosition{DOOR.map, DOOR.goal})){
        return "Expected the door open and the player at the goal, ended at " + to_string(out.position);
    }
    return true;
}

UnitTestResult test_step_sim_statue_switch(){
    GateStateOut out;
    const std::string failure = on_overworld_sim(STATUE, false, out);
    if (!failure.empty()){
        return "The step did not press the statue: " + failure;
    }
    if (!out.gates.switch_on("mansion") || !(out.position == MapPosition{STATUE.map, STATUE.goal})){
        return "Expected the switch on and the player at the goal, ended at " + to_string(out.position);
    }
    return true;
}

const std::vector<GatesCase> GATES_CASES{
    {.input = {.name = "story maps: a closed door blocks without the Card Key", .body = test_maps_sim_door_blocks}, .expectation = {}},
    {.input = {.name = "story maps: the door opens with the Card Key and stays open", .body = test_maps_sim_door_opens}, .expectation = {}},
    {.input = {.name = "story maps: the statue's switch opens its barrier", .body = test_maps_sim_statue_switch}, .expectation = {}},
    {.input = {.name = "overworld sim: a closed door blocks without the Card Key", .body = test_step_sim_door_blocks}, .expectation = {}},
    {.input = {.name = "overworld sim: the door opens with the Card Key", .body = test_step_sim_door_opens}, .expectation = {}},
    {.input = {.name = "overworld sim: the statue's switch opens its barrier", .body = test_step_sim_statue_switch}, .expectation = {}},
};

} // namespace


void add_tests_AutoStoryGates(UnitTestDatabase& database){
    for (const GatesCase& item : GATES_CASES){
        database.add<Test_AutoStoryGates>(item);
    }
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
