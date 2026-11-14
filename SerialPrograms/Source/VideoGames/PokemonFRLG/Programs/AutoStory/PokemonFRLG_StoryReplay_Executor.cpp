/*  Native Checkpoint Engine with Scripted Game IO
 *  From: https://github.com/PokemonAutomation/
 */
#include <format>
#include <limits>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_StoryReplay_Executor.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryReplayInternal{
namespace{
struct ReplayStopped{};
Pokemon::Pose native_pose(const OverworldPose& pose){
    return Pokemon::tile_pose(pose.position.map, {pose.position.point}, pose.facing);
}
JsonValue action_json(const StoryStep& event, const OverworldPose& from, const OverworldPress& next){
    JsonObject action;
    action["event"] = event.name;
    action["map"] = from.position.map;
    action["direction"] = direction_name(Pokemon::story_arg<Direction>(event.args, "direction"));
    if (next.warped || next.pose.position.map != from.position.map){
        action["kind"] = "transition";
        action["to_map"] = next.pose.position.map;
        action["via"] = next.warped ? "warp" : "edge";
    }else{
        action["kind"] = "walk";
        action["tiles"] = 1;
    }
    return action;
}
void check_candidate_move(const JsonObject& command, const OverworldPose& from, const OverworldPress& next){
    const std::string kind = command.get_string_throw("kind");
    if (kind == "walk"){
        if (next.warped || next.pose.position.map != from.position.map){
            throw ParseException("story replay: walk crossed an undeclared map transition");
        }
    }else if (command.get_string_throw("to_map") != next.pose.position.map
        || command.get_string_throw("via") != (next.warped ? "warp" : "edge")){
        throw ParseException("story replay: supplied transition disagrees with native rules");
    }
}
//  The engine's pose as {"map", "tiles": [[x, y], ...], "facing"}: its
//  tiles whatever its certainty.
JsonValue engine_pose_json(const Pokemon::Pose& pose){
    JsonObject result;
    result["map"] = pose.map;
    JsonArray tiles;
    for (const Pokemon::TilePos& tile : Pokemon::tiles_of(pose)){
        JsonArray pair;
        pair.push_back(tile.x);
        pair.push_back(tile.y);
        tiles.push_back(std::move(pair));
    }
    result["tiles"] = std::move(tiles);
    if (const std::optional<Direction> facing = Pokemon::facing_of(pose)){
        result["facing"] = direction_name(*facing);
    }
    return result;
}
} // namespace
ReplayExecutor::ReplayExecutor(const ReplayData& data, const JsonValue& scenario)
    : m_data(&data), m_scenario(&scenario.to_object_throw())
    , m_pose(read_pose(m_scenario->get_object_throw("entry").get_value_throw("pose"), data.maps))
    , m_table(make_story_action_table([this](StoryActionId id){ return replay_action(id); }))
    , m_executor(m_table, m_guard)
    , m_interrupts(step_interrupts([this](bool){ return look(); }, interrupt_actions()))
{
    m_executor.set_interrupts(&m_interrupts);
}
bool ReplayStepGuard::stops_on_failure(const StoryStep&) const{
    return true;
}
Pokemon::StoryAction ReplayExecutor::replay_action(StoryActionId id){
    switch (id){
    case StoryActionId::ROUTE:
        return {.act = [this](const StoryStep& step, Pokemon::RunState&){ return replay_route(step); }};
    case StoryActionId::STEP:
        return {.act = [this](const StoryStep& step, Pokemon::RunState&){ return replay_step(step); }};
    case StoryActionId::MASH:
        return {.act = [this](const StoryStep& step, Pokemon::RunState&){ return replay_scene(step); }};
    default:
        return {.act = [](const StoryStep& step, Pokemon::RunState&){ return Pokemon::unsupported_step(step); }};
    }
}
const JsonObject* ReplayExecutor::signal() const{
    return m_scenario->get_object_throw("signals").get_object(m_active);
}
const JsonObject& ReplayExecutor::parameters(const StoryStep& step) const{
    return m_data->binding.to_object_throw().get_object_throw("capabilities").get_object_throw(step.name).get_object_throw("parameters");
}
Pokemon::Checkpoint ReplayExecutor::build(){
    Pokemon::Checkpoint checkpoint = Pokemon::build_story_checkpoint(m_data->row, *this);
    checkpoint.start.push_back(Pokemon::condition("scripted_entry", [this](Pokemon::RunState&){
        const auto expected = authored_pose(Pokemon::pose_of(*m_data->row.start), m_data->maps);
        return m_scenario->get_object_throw("entry").get_string_throw("screen") == "overworld"
            && pose_json(m_pose, m_pose.facing) == pose_json(expected, expected.facing);
    }));
    return checkpoint;
}
Pokemon::Outcome ReplayExecutor::run(const StoryStep& step, Pokemon::RunState& state){
    m_active = step.name;
    Pokemon::Outcome outcome = m_executor.run(step, state);
    if (outcome.succeeded()){
        m_completed.push_back(step.name);
    }else if (!m_known){
        state.pose = Pokemon::Pose{};
    }
    return outcome;
}
void ReplayExecutor::lose_pose(std::string basis){
    m_known = false;
    m_basis = std::move(basis);
}
Pokemon::ScreenLook ReplayExecutor::look(){
    const WallClock start = WallClock() + std::chrono::hours(1);
    Pokemon::ScreenLook out;
    out.log.record({}, start);
    ReplayScreen screen{{}, 400};
    const JsonObject* between = m_scenario->get_object("between_steps");
    const JsonObject* shown = between ? between->get_object(m_active) : nullptr;
    if (shown && m_shown.insert(m_active).second){
        screen = between_steps_screen(shown->get_string_throw("kind"));
    }
    for (const auto& [ms, events] : screen.frames){
        out.log.record(events, start + Milliseconds(ms));
    }
    out.window = {.start = start, .end = start + Milliseconds(screen.look_ms)};
    return out;
}
StepInterruptActions ReplayExecutor::interrupt_actions(){
    const auto recorded = [this](bool warp){
        return [this, warp](const Pokemon::InterruptVerdict& verdict, Pokemon::RunState&){
            JsonObject action;
            action["event"] = m_active;
            action["kind"] = "interrupt";
            action["interrupt"] = Pokemon::interrupt_kind_name(verdict.kind);
            m_actions.push_back(std::move(action));
            if (warp){
                lose_pose("unknown-after-warp");
            }
            return Pokemon::outcome_ok();
        };
    };
    return {.wild = recorded(false), .trainer = recorded(false), .dialog = recorded(false), .warp = recorded(true)};
}
void ReplayExecutor::observe_interruption(const OverworldPose& predicted){
    m_candidates.push_back(pose_json(m_pose, std::nullopt));
    m_candidates.push_back(pose_json(predicted, std::nullopt));
    m_interrupted = true;
    lose_pose("unknown-after-interruption");
}
ReplayLeg ReplayExecutor::prepare_leg(const JsonValue& value, OverworldPose& current) const{
    const JsonObject& command = value.to_object_throw();
    const std::string kind = command.get_string_throw("kind");
    if (kind == "walk"){
        fields(value, {"event", "kind", "map", "direction", "tiles"});
    }else if (kind == "transition"){
        fields(value, {"event", "kind", "map", "direction", "to_map", "via"});
    }else if (kind == "turn"){
        fields(value, {"event", "kind", "map", "direction"});
    }else{
        throw ParseException("story replay: unsupported supplied movement (including hops)");
    }
    if (command.get_string_throw("event") != m_active || command.get_string_throw("map") != current.position.map){
        throw ParseException("story replay: supplied route has wrong event/map");
    }
    const Direction facing = direction(command.get_string_throw("direction"));
    const int64_t tiles = kind == "walk" ? command.get_integer_throw("tiles") : kind == "turn" ? 0 : 1;
    if (tiles < (kind == "turn" ? 0 : 1) || tiles > 4096){
        throw ParseException("story replay: invalid supplied leg length");
    }
    RouteStep step{.action = kind == "turn" ? RouteAction::FACE : RouteAction::WALK, .direction = facing, .tiles = (int)tiles};
    for (int64_t n = 0; n < tiles; n++){
        const auto next = predict_overworld_press(m_data->maps, current, facing);
        if (!next){
            throw ParseException("story replay: native model rejects supplied route");
        }
        check_candidate_move(command, current, *next);
        if (!next->warped && next->pose.position.map != current.position.map){
            step.connection_steps.push_back((int)n + 1);
        }
        step.walk_only = step.walk_only || !map_named(m_data->maps, current.position.map).allow_running;
        current = next->pose;
        step.warps = step.warps || next->warped;
        step.wild = step.wild || is_tall_grass(map_named(m_data->maps, current.position.map), current.position.point);
    }
    if (kind == "turn"){
        current.facing = facing;
    }
    step.end = current.position;
    step.facing_after = current.facing;
    return ReplayLeg{.step = step, .action = value.clone()};
}
std::vector<RouteStep> ReplayExecutor::prepare_route(const StoryStep& step){
    const JsonArray& commands = m_data->candidate.to_object_throw().get_object_throw("routes").get_array_throw(step.name);
    if (commands.size() > 4096){
        throw ParseException("story replay: supplied route exceeds action bound");
    }
    OverworldPose current = m_pose;
    std::vector<RouteStep> route;
    m_legs.clear();
    for (const JsonValue& command : commands){
        m_legs.emplace_back(prepare_leg(command, current));
        route.push_back(m_legs.back().step);
    }
    const OverworldPose goal = authored_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target")), m_data->maps);
    if (!(pose_json(current, current.facing) == pose_json(goal, goal.facing))){
        throw ParseException("story replay: supplied candidate misses its authored target");
    }
    return route;
}
void ReplayExecutor::default_plan(const StoryStep& step){
    const OverworldPose goal = authored_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target")), m_data->maps);
    const auto planned = plan_overworld_route(m_data->maps, m_pose.position, {goal.position, goal.facing}, story_route_costs(), m_pose.facing);
    JsonObject plan;
    plan["policy"] = "native-story-route-costs";
    plan["exact_python_leg_comparison"] = "not-comparable-different-planner-policies";
    JsonArray legs;
    if (planned){
        const MapPosition actual = follow_overworld_route(m_data->maps, m_pose.position, *planned);
        if (!(actual == goal.position)){
            throw ParseException("story replay: native default plan did not reach target");
        }
        for (const RouteStep& leg : *planned){
            legs.push_back(to_string(leg));
        }
    }
    plan["status"] = planned ? "planned-and-replayed" : "no-route";
    plan["legs"] = std::move(legs);
    m_native_plans[step.name] = std::move(plan);
}
void ReplayExecutor::timed_plan(const StoryStep& step, const std::vector<RouteStep>& route){
    //  Independent control: real simulated holds, fades and connection stalls.
    //  Scripted uncertain encounters below are not claimed as timed observations.
    if (signal()){
        return;
    }
    OverworldSim sim(m_data->maps, m_pose.position, m_pose.facing);
    const RoutePlanning planning{.maps = &m_data->maps, .start = m_pose.position,
        .label = step.name, .wall_check = WallCheck::SKIP};
    const std::string failure = OverworldSimWalker(sim).run(route, planning);
    const auto goal = authored_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target")), m_data->maps);
    const bool matched = failure.empty() && sim.state().position == goal.position
        && sim.state().facing == goal.facing && sim.state().violations.empty();
    JsonObject result;
    result["status"] = matched ? "complete" : "failed";
    result["profile"] = "native-OverworldSimWalker-no-interruption-control";
    result["failure"] = failure;
    result["position"] = to_string(sim.state().position);
    result["facing"] = direction_name(sim.state().facing);
    JsonArray violations;
    for (const std::string& violation : sim.state().violations){
        violations.push_back(violation);
    }
    result["violations"] = std::move(violations);
    m_timed_plans[step.name] = std::move(result);
}
StepOutcome ReplayExecutor::walk_leg(const RouteStep& step){
    m_actions.push_back(m_legs[m_next_leg++].action.clone());
    const OverworldPose start = m_pose;
    for (int n = 0; n < step.tiles; n++){
        const auto next = predict_overworld_press(m_data->maps, m_pose, step.direction);
        if (!next){
            throw ParseException("story replay: supplied walk became blocked");
        }
        const JsonObject* s = signal();
        if (s && pose_json(m_pose, m_pose.facing) == s->get_value_throw("from")
            && s->get_string_throw("direction") == direction_name(step.direction)){
            observe_interruption(next->pose);
            return StepOutcome{.end = StepEnd::STOPPED};
        }
        m_pose = next->pose;
        m_moves++;
    }
    m_pose.facing = step.facing_after;
    const MapPosition followed = follow_overworld_route(m_data->maps, start.position, {step});
    if (!(followed == m_pose.position)){
        throw ParseException("story replay: walker and independent native route replay disagree");
    }
    m_basis = "model-prediction";
    return StepOutcome{};
}
Pokemon::Outcome ReplayExecutor::replay_route(const StoryStep& step){
    default_plan(step);
    const std::vector<RouteStep> route = prepare_route(step);
    size_t moves = 0;
    for (const RouteStep& leg : route){
        moves += (size_t)leg.tiles;
    }
    if (moves > (size_t)m_data->binding.to_object_throw().get_integer_throw("max_route_moves")){
        return Pokemon::outcome_failed("route_move_budget_exceeded");
    }
    timed_plan(step, route);
    m_next_leg = 0;
    RouteCalls calls;
    calls.walk = [this](const RouteStep& leg, std::optional<Direction>){ return walk_leg(leg); };
    calls.log = [this](const std::string& line){ m_route_logs.push_back(line); };
    calls.fail = [](const std::string&){ throw ReplayStopped{}; };
    calls.hold_to_warp = [](const RouteStep&){ throw ReplayStopped{}; };
    try{
        run_route(route, m_pose.facing, nullptr, calls);
    }catch (const ReplayStopped&){
        return Pokemon::outcome_failed(m_interrupted ? "encounter_requires_recovery" : "native_route_stopped");
    }
    if (signal() && !m_interrupted){
        throw ParseException("story replay: physical encounter injection was not reached");
    }
    return Pokemon::outcome_ok(native_pose(m_pose));
}
Pokemon::Outcome ReplayExecutor::replay_step(const StoryStep& step){
    const auto next = predict_overworld_press(m_data->maps, m_pose, Pokemon::story_arg<Direction>(step.args, "direction"));
    if (!next){
        lose_pose("unknown-after-transition");
        return Pokemon::outcome_failed("step_unresolved");
    }
    m_actions.push_back(action_json(step, m_pose, *next));
    if (signal()){
        lose_pose("unknown-after-transition");
        return Pokemon::outcome_failed("transition_unconfirmed");
    }
    m_pose = next->pose;
    m_moves++;
    m_basis = "model-prediction";
    return Pokemon::outcome_ok(native_pose(m_pose));
}
Pokemon::Outcome ReplayExecutor::replay_scene(const StoryStep& step){
    JsonObject action;
    action["event"] = step.name;
    action["kind"] = "scene";
    action["capability"] = "scripted-scene/1";
    m_actions.push_back(std::move(action));
    const JsonObject* s = signal();
    const auto& required = parameters(step).get_value_throw("require_received");
    const auto expected = authored_pose(Pokemon::pose_of(*step.pose_after), m_data->maps);
    if (!s || s->get_string_throw("screen") != "overworld" || s->get_value_throw("pose") != pose_json(expected, expected.facing)
        || s->get_value_throw("received") != required){
        lose_pose("unknown-after-scene");
        return Pokemon::outcome_failed("scene_evidence_missing_or_mismatched");
    }
    m_pose = read_pose(s->get_value_throw("pose"), m_data->maps);
    m_basis = "scripted-observation";
    for (const auto& [item, count] : s->get_object_throw("received")){
        const int64_t previous = m_received.get_integer_default(item);
        const int64_t received = count.to_integer_throw();
        if (previous > std::numeric_limits<int64_t>::max() - received){
            throw ParseException("story replay: accumulated receipt overflows int64");
        }
        m_received[item] = previous + received;
    }
    return Pokemon::outcome_ok(native_pose(m_pose));
}
JsonValue ReplayExecutor::report(const Pokemon::CheckpointReport& result, const Pokemon::RunState& state, const JsonArray& logs) const{
    JsonObject root;
    root["format"] = "pokesim-native-checkpoint-replay/1";
    root["status"] = result.completed ? "model_complete" : "stopped";
    root["checkpoint"] = m_data->row.id;
    root["profile_sha256"] = m_data->binding_hash;
    root["profile_map_revision"] = m_data->binding.to_object_throw().get_string_throw("map_revision");
    root["native_unbound"] = m_data->native_unbound.clone();
    root["native_source_revision"] = m_data->source_revision;
    root["route_source"] = "supplied-python-candidate-not-native-planner";
    root["native_parity"] = "diagnostic-profile-only-not-live-executor";
    root["failure"] = result.completed ? JsonValue() : JsonValue(result.failure.failure == "start_failed" ? "entry_requirement_failed" : result.failure.failure);
    root["active_event"] = result.completed || result.failed_event == 0 ? JsonValue() : JsonValue(m_active);
    root["completed_events"] = m_completed.clone();
    root["pose"] = m_known ? pose_json(m_pose, m_pose.facing) : JsonValue();
    root["pose_basis"] = m_basis;
    root["candidates"] = m_candidates.clone();
    root["received"] = m_received.clone();
    root["save_performed"] = false;
    root["policy_basis"] = "synthetic-offline-policy";
    root["initialization"] = "existing-engine-from_save-after-scripted-entry-check";
    //  The engine's pose as a grid pose (its tiles, whatever its certainty),
    //  or null when unknown.
    root["engine_pose"] = state.pose.certainty == Pokemon::PoseCertainty::UNKNOWN ? JsonValue()
        : engine_pose_json(state.pose);
    root["engine_failure"] = result.failure.failure;
    root["engine_log"] = logs.clone();
    root["walker_log"] = m_route_logs.clone();
    root["actions"] = m_actions.clone();
    root["native_story_plans"] = m_native_plans.clone();
    root["native_timing_controls"] = m_timed_plans.clone();
    root["confirmed_model_moves"] = m_moves;
    root["production_save_retry_policy"] = "unbound-no-console-options-exported";
    return root;
}
} // namespace StoryReplayInternal
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
