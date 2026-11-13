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
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldGatedRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryGates_Tests.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_StoryReplay.h"
#include "PokemonFRLG_StoryReplay_IO.h"

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
    std::string_view map;
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
    state.pose = to_pose(StoryPose{std::string(scene.map), scene.start, Direction::EAST});
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
    return sim.run(route_step(std::string(scene.map), scene.goal), state);
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
    OverworldSim sim(scene_maps(), MapPosition{std::string(scene.map), scene.start}, Direction::EAST, script);
    StoryPose at{std::string(scene.map), scene.start, Direction::EAST};
    const std::string failure = row_step_on_sim(sim, at, route_step(std::string(scene.map), scene.goal), "test");
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
    if (!out.gates.opened.contains("SilphCo_5F/door_1") || !(out.position == MapPosition{std::string(DOOR.map), DOOR.goal})){
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
    if (!out.gates.switch_on("mansion") || !(out.position == MapPosition{std::string(STATUE.map), STATUE.goal})){
        return "Expected the switch on and the player at the goal, ended at " + to_string(out.position);
    }
    return true;
}

//  A recorded route replayed on the real maps: the native planner's route
//  through the gate as the replay's supplied commands (what a recording of
//  the walk holds), replayed by the story engine on the pinned maps with the
//  run's flags (the Card Key) and the gates the route's commands change.
constexpr std::string_view REVISION = "0123456789abcdef0123456789abcdef01234567";

const MapData& scene_map(const Scene& scene){
    for (const MapData& map : scene_maps()){
        if (map.name == scene.map){
            return map;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown scene map.");
}

JsonObject tile_json(const Scene& scene, TilePos tile, std::string_view facing){
    JsonArray pair;
    pair.push_back(tile.x);
    pair.push_back(tile.y);
    JsonArray tiles;
    tiles.push_back(std::move(pair));
    JsonObject where;
    where["kind"] = "tiles";
    where["tiles"] = std::move(tiles);
    where["facing"] = std::string(facing);
    JsonObject pose;
    pose["certainty"] = "tile";
    pose["map"] = std::string(scene.map);
    pose["where"] = std::move(where);
    return pose;
}

//  The planned route as supplied commands: walks, turns, and the gate's A press.
JsonArray recorded_commands(const Scene& scene, const std::vector<RouteStep>& route){
    JsonArray commands;
    for (const RouteStep& step : route){
        JsonObject command;
        command["event"] = "cross";
        command["map"] = std::string(scene.map);
        command["direction"] = std::string(direction_name(step.direction));
        switch (step.action){
        case RouteAction::WALK:
            command["kind"] = "walk";
            command["tiles"] = step.tiles;
            break;
        case RouteAction::FACE:
            command["kind"] = "turn";
            break;
        case RouteAction::OPEN_GATE:
            command["kind"] = "open_gate";
            break;
        case RouteAction::PRESS_SWITCH:
            command["kind"] = "press_switch";
            break;
        default:
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "The scene's route has a step the replay does not take.");
        }
        commands.push_back(std::move(command));
    }
    return commands;
}

//  The replay of the scene's checkpoint with the recorded route. The scenario starts with
//  the Card Key if `card_key`; the recording is made with it either way.
//  Throws what the replay rejects.
JsonValue replay_scene(const Scene& scene, bool card_key, bool recorded_card_key = true){
    const MapData& map = scene_map(scene);
    const Pokemon::GateRules rules{.card_key_held = recorded_card_key};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route_gated(
        scene_maps(), MapPosition{std::string(scene.map), scene.start}, RouteGoal{MapPosition{std::string(scene.map), scene.goal}, std::nullopt},
        Pokemon::GateState(), rules, default_route_costs(), Direction::EAST
    );
    if (!route){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No route across the scene to record.");
    }
    const Direction facing_after = route->back().direction;

    JsonObject start_pose = tile_json(scene, scene.start, "east");
    JsonObject target = tile_json(scene, scene.goal, direction_name(facing_after));
    JsonObject event;
    event["id"] = "cross";
    event["kind"] = "route";
    event["target"] = std::move(target);
    event["why"] = "across the gate";
    JsonArray events;
    events.push_back(std::move(event));
    JsonObject checkpoint;
    checkpoint["id"] = "99.01";
    checkpoint["title"] = "Gate replay";
    checkpoint["start"] = std::move(start_pose);
    checkpoint["events"] = std::move(events);
    JsonObject capture;
    capture["format"] = "pokesim-native-checkpoint-export/1";
    capture["game"] = "frlg";
    capture["checkpoint"] = std::move(checkpoint);
    capture["source_revision"] = std::string(REVISION);
    capture["unbound"] = JsonArray();
    const std::string capture_text = capture.dump(-1);

    std::string map_hash;
    for (const auto& [name, text] : overworld_map_json()){
        if (name == scene.map){
            map_hash = StoryReplayInternal::digest(text);
        }
    }
    JsonObject capture_pin;
    capture_pin["file"] = "gate-replay.json";
    capture_pin["sha256"] = StoryReplayInternal::digest(capture_text);
    capture_pin["source_revision"] = std::string(REVISION);
    JsonObject maps;
    maps[std::string(scene.map)] = map_hash;
    JsonObject capability;
    capability["capability"] = "frlg-runtime-route/1";
    capability["parameters"] = JsonObject();
    JsonObject capabilities;
    capabilities["cross"] = std::move(capability);
    JsonObject costs;
    costs["tile"] = 1;
    costs["turn"] = 2;
    costs["grass"] = 5;
    costs["warp"] = 1;
    JsonObject policy;
    policy["basis"] = "synthetic-offline-policy";
    policy["entry"] = "scripted-observation";
    policy["attempts"] = 1;
    policy["failure"] = "stop";
    policy["save"] = "never";
    JsonObject binding;
    binding["format"] = "pokesim-story-replay-binding/1";
    binding["capture"] = std::move(capture_pin);
    binding["map_revision"] = std::string(REVISION);
    binding["maps"] = std::move(maps);
    binding["capabilities"] = std::move(capabilities);
    binding["costs"] = std::move(costs);
    binding["max_route_moves"] = 256;
    binding["policy"] = std::move(policy);
    const std::string binding_text = binding.dump(-1);

    JsonObject routes;
    routes["cross"] = recorded_commands(scene, *route);
    JsonObject candidate;
    candidate["format"] = "pokesim-route-candidate/1";
    candidate["binding_sha256"] = StoryReplayInternal::digest(binding_text);
    candidate["checkpoint"] = "99.01";
    candidate["routes"] = std::move(routes);

    JsonObject entry_pose;
    entry_pose["map"] = std::string(scene.map);
    JsonArray tile;
    tile.push_back(scene.start.x);
    tile.push_back(scene.start.y);
    entry_pose["tile"] = std::move(tile);
    entry_pose["facing"] = "east";
    entry_pose["elevation"] = elevation_at(map, scene.start);
    JsonObject entry;
    entry["basis"] = "scripted";
    entry["screen"] = "overworld";
    entry["pose"] = std::move(entry_pose);
    JsonObject scenario;
    scenario["entry"] = std::move(entry);
    scenario["signals"] = JsonObject();
    JsonArray flags;
    if (card_key){
        flags.push_back("card_key_held");
    }
    scenario["start_flags"] = std::move(flags);
    return replay_story_checkpoint(capture_text, binding_text, candidate.dump(-1), JsonValue(std::move(scenario)));
}

//  The replay completed the checkpoint and its actions have `kind` (the gate's command).
std::string completed_with(const JsonValue& report, std::string_view kind){
    const JsonObject& root = report.to_object_throw();
    if (root.get_string_throw("status") != "model_complete"){
        return "The replay stopped: " + report.dump(-1);
    }
    for (const JsonValue& action : root.get_array_throw("actions")){
        if (action.to_object_throw().get_string_throw("kind") == kind){
            return "";
        }
    }
    return std::format("The replay's actions have no {}.", kind);
}

UnitTestResult test_replay_door_with_card_key(){
    const std::string failure = completed_with(replay_scene(DOOR, true), "open_gate");
    if (!failure.empty()){
        return failure;
    }
    return true;
}

//  The recording was made with the Card Key; the run's scenario has none.
UnitTestResult test_replay_door_without_card_key(){
    try{
        const std::string failure = completed_with(replay_scene(DOOR, false), "open_gate");
        if (failure.empty()){
            return "The replay opened a card-key door for a run without the Card Key.";
        }
    }catch (const ParseException& e){
        if (e.message().find("card-key") == std::string::npos){
            return "Unexpected rejection: " + e.message();
        }
    }
    return true;
}

UnitTestResult test_replay_statue_switch(){
    const std::string failure = completed_with(replay_scene(STATUE, false), "press_switch");
    if (!failure.empty()){
        return failure;
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
    {.input = {.name = "replay: a recorded route opens Silph Co.'s card-key door", .body = test_replay_door_with_card_key}, .expectation = {}},
    {.input = {.name = "replay: the recorded door stays shut without the Card Key", .body = test_replay_door_without_card_key}, .expectation = {}},
    {.input = {.name = "replay: a recorded route presses the Mansion's statue", .body = test_replay_statue_switch}, .expectation = {}},
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
