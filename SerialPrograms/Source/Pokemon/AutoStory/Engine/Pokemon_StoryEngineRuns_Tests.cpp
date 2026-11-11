/*  Story Engine Runs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a game needs from the engine to drop its own workarounds (the PLZA
 *  move, #335): a scripted backend for any pose type, the move stick and
 *  the heading reading, the choice and interact events, a screen machine
 *  over observations, the step in an action's call, the checkpoint in the
 *  save, the segment and verify loops, console-only actions, declared poses
 *  read as the game's pose type, and tile actions only for tile games.
 *  Then what PLZA found on the engine: readers opt in to the postcondition
 *  check, inputs logged by button name, waiting states may stay still, and
 *  the stick's release.
 *
 */

#include <algorithm>
#include <chrono>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/ScreenMachine/ScreenMachine.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "Pokemon_StoryEngine.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryEngineRuns_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

using namespace std::chrono_literals;


std::string joined(const std::vector<std::string>& items){
    std::string text;
    for (const std::string& item : items){
        text += (text.empty() ? "" : ", ") + item;
    }
    return text;
}

template <StoryPose PoseType>
bool has_line(const FakeStoryBackend<PoseType>& backend, std::string_view line){
    for (const std::string& each : backend.lines()){
        if (each == line){
            return true;
        }
    }
    return false;
}

template <StoryPose PoseType>
size_t count_lines(const FakeStoryBackend<PoseType>& backend, std::string_view line){
    size_t count = 0;
    for (const std::string& each : backend.lines()){
        count += each == line ? 1 : 0;
    }
    return count;
}

template <StoryPose PoseType>
std::string expect_inputs(const FakeStoryBackend<PoseType>& backend, std::string_view expected){
    const std::string got = joined(backend.inputs());
    return got == expected ? "" : std::format("expected inputs \"{}\", got \"{}\"", expected, got);
}

template <StoryPose PoseType>
Observation<PoseType> look(std::initializer_list<ScreenEvent> events){
    Observation<PoseType> observation;
    observation.events = ScreenEvents(events);
    return observation;
}

StoryStep game_step(std::string name, std::string action, std::string why = ""){
    StoryStep step;
    step.kind = StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.action = std::move(action);
    return step;
}

StoryCheckpoint checkpoint_of(std::string id, std::vector<StoryStep> steps){
    StoryCheckpoint checkpoint;
    checkpoint.id = std::move(id);
    checkpoint.steps = std::move(steps);
    return checkpoint;
}

//  `checkpoint` in `phase`, verified with its steps as they are now.
template <StoryPose PoseType>
StoryCheckpoint in_phase(StoryCheckpoint checkpoint, StoryPhase phase, const StoryEngine<PoseType>& engine){
    checkpoint.metadata.phase = phase;
    checkpoint.metadata.verified = StoryVerification{
        .steps_hash = engine.steps_hash(checkpoint), .build = "abc", .date = "2026-11-10", .runs = "3/3",
    };
    return checkpoint;
}

//  A GAME action of no args.
template <StoryPose PoseType>
StoryAction<PoseType> game_action(std::string name, StoryActionHandler<PoseType> handler){
    return {.spec = {.kind = StoryStepKind::GAME, .name = std::move(name), .args = {}}, .handler = std::move(handler)};
}

//  A game whose "flaky" action fails its first `failures` calls, whose
//  save logs "[Test] save <checkpoint>" and whose reload logs "[Test] reload".
template <StoryPose PoseType>
StoryGame<PoseType> flaky_game(size_t failures){
    StoryGame<PoseType> game;
    game.id = "test";
    std::shared_ptr<size_t> calls = std::make_shared<size_t>(0);
    game.actions.push_back(game_action<PoseType>("flaky", [calls, failures](StoryBackend<PoseType>&, const StoryActionCall&, RunState&){
        return (*calls)++ < failures ? outcome_failed("flaky_failed") : outcome_ok();
    }));
    game.actions.push_back(game_action<PoseType>("fine", [](StoryBackend<PoseType>&, const StoryActionCall&, RunState&){
        return outcome_ok();
    }));
    game.save = [](StoryBackend<PoseType>& backend, const StoryCheckpoint& checkpoint){
        backend.log(std::format("[Test] save {}", checkpoint.id));
        return outcome_ok();
    };
    game.reload = [](StoryBackend<PoseType>& backend){
        backend.log("[Test] reload");
        return outcome_ok();
    };
    return game;
}


//  Gap 1 and 9: a zone game's checkpoint read from JSON whose start names
//  only its zone, run on the scripted backend of its pose type.
std::string zone_checkpoint_runs(CancellableScope& scope){
    StoryGame<ZonePose> game = flaky_game<ZonePose>(0);
    FakeStoryBackend<ZonePose> backend({.looks = {look<ZonePose>({ScreenEvent::CAN_MOVE})}}, scope);
    StoryEngine<ZonePose> engine(game, backend);
    const StoryCheckpoint checkpoint = read_story_checkpoint(parse_json(R"({
        "id": "03.01", "title": "Down to the lobby", "start": {"place": "HotelZ", "spot": "Room", "heading": 180},
        "events": [{"id": "walk", "kind": "game_action", "action": "fine", "declared_after": {"place": "HotelZLobby"}}]
    })"), "zone", engine.registry().specs());
    RunState state;
    const StoryEngineReport report = engine.run_checkpoint(checkpoint, SavePoint::RUN_END, state);
    if (report.verdict != CheckpointVerdict::PASSED){
        return std::format("expected a pass, got {}", to_string(report.run.failure));
    }
    return state.pose.map == "HotelZLobby" ? "" : std::format("expected the run's pose in HotelZLobby, got \"{}\"", state.pose.map);
}

//  Gap 9: a declared tile pose is no zone.
std::string zone_rejects_tiles(CancellableScope& scope){
    StoryGame<ZonePose> game = flaky_game<ZonePose>(0);
    FakeStoryBackend<ZonePose> backend({}, scope);
    StoryEngine<ZonePose> engine(game, backend);
    StoryCheckpoint checkpoint = checkpoint_of("03.01", {game_step("walk", "fine")});
    checkpoint.start = TileDeclared{.map = "HotelZ", .tiles = {{1, 2}}, .facing = std::nullopt};
    const std::vector<std::string> lint = engine.registry().lint(checkpoint);
    if (lint.size() != 1 || !lint[0].starts_with("03.01 start: \"HotelZ\" is a map")){
        return std::format("expected one start problem, got \"{}\"", joined(lint));
    }
    RunState state;
    const StoryEngineReport report = engine.run_checkpoint(checkpoint, SavePoint::RUN_END, state);
    if (report.run.failure.failure != "lint_failed"){
        return std::format("expected lint_failed, got {}", to_string(report.run.failure));
    }
    return expect_inputs(backend, "");
}

//  A tile game's checkpoint that declares a place, at its start or after
//  a step, fails the lint.
std::string grid_rejects_places(CancellableScope&){
    const StoryGame<GridPose> game = flaky_game<GridPose>(0);
    const StoryActionRegistry<GridPose> registry(game);
    StoryCheckpoint checkpoint = checkpoint_of("00.01", {game_step("walk", "fine")});
    checkpoint.start = ZoneDeclared{.place = "PalletTown", .spot = std::nullopt, .heading = std::nullopt};
    checkpoint.steps[0].pose_after = ZoneDeclared{.place = "Route1", .spot = "Gate", .heading = 90};
    const std::string lint = joined(registry.lint(checkpoint));
    const std::string expected =
        "00.01 start: \"PalletTown\" is a place, but this game's poses are tiles: declare a map, tiles and a facing, "
        "00.01 step 1 walk: declared_after: \"Route1\" is a place, but this game's poses are tiles: declare a map, tiles and a facing";
    return lint == expected ? "" : std::format("expected \"{}\", got \"{}\"", expected, lint);
}

//  A zone game may not use tile args: not in a step (a route's GRID_POSE
//  target), not in its own actions (a LEGS arg). Timed legs are fine.
std::string zone_rejects_tile_args(CancellableScope&){
    StoryGame<ZonePose> game;
    game.actions.push_back({
        .spec = {.kind = StoryStepKind::GAME, .name = "walk", .args = {
            {.key = "legs", .type = StoryArgType::LEGS, .required = true, .fallback = std::nullopt, .why = ""},
        }},
        .handler = nullptr,
    });
    game.actions.push_back({
        .spec = {.kind = StoryStepKind::GAME, .name = "timed_walk", .args = {
            {.key = "legs", .type = StoryArgType::TIMED_LEGS, .required = true, .fallback = std::nullopt, .why = ""},
        }},
        .handler = nullptr,
    });
    const StoryActionRegistry<ZonePose> registry(game);
    StoryStep route;
    route.kind = StoryStepKind::ROUTE;
    route.name = "route";
    route.args = {{"target", GridPose{.map = "HotelZ", .tiles = {}, .facing = std::nullopt}}};
    StoryStep timed;
    timed.kind = StoryStepKind::GAME;
    timed.name = "timed";
    timed.action = "timed_walk";
    timed.args = {{"legs", std::vector<StoryTimedLeg>{{.turn = 90, .ms = 1200}}}};
    const std::string lint = joined(registry.lint(checkpoint_of("03.01", {route, timed})));
    const std::string expected =
        "action walk takes a tile argument \"legs\", but this game has no tiles, "
        "03.01 step 1 route: argument \"target\" is a tile one, but this game has no tiles";
    return lint == expected ? "" : std::format("expected \"{}\", got \"{}\"", expected, lint);
}

//  Gap 10: no tile actions in a zone game, so a legs step fails the lint;
//  a tile game keeps them.
std::string tile_actions_only_with_tiles(CancellableScope&){
    StoryStep legs;
    legs.kind = StoryStepKind::LEGS;
    legs.name = "legs";
    legs.args = {{"legs", std::vector<StoryLeg>{{Direction::NORTH, 1}}}};
    StoryStep face;
    face.kind = StoryStepKind::STEP;
    face.name = "face";
    face.action = "face";
    face.args = {{"direction", Direction::EAST}};
    const StoryGame<ZonePose> zone_game;
    const StoryActionRegistry<ZonePose> zone(zone_game);
    if (joined(zone.check(legs)) != "unknown action \"\" for LEGS" || joined(zone.check(face)) != "unknown action \"face\" for STEP"){
        return std::format("expected the zone game to have no legs or face, got \"{}\", \"{}\"", joined(zone.check(legs)), joined(zone.check(face)));
    }
    const StoryGame<GridPose> grid_game;
    const StoryActionRegistry<GridPose> grid(grid_game);
    if (!grid.check(legs).empty() || !grid.check(face).empty()){
        return "expected the tile game to keep legs and face";
    }
    return "";
}

//  Gap 2: the move stick at an angle, and the heading reading; a turn lets
//  go of the stick for its release.
std::string stick_and_heading(CancellableScope& scope){
    Observation<ZonePose> facing_east;
    facing_east.readings[std::string(STORY_HEADING)] = 90;
    FakeStoryBackend<ZonePose> backend({.looks = {facing_east, {}}}, scope);
    backend.stick(45, 150ms, 0ms, StoryPace::WALK);
    backend.stick(-90, 300ms, 0ms, StoryPace::RUN);
    backend.stick(90, 150ms, 300ms, StoryPace::WALK);
    if (story_heading(backend.observe()) != 90){
        return "expected the heading 90";
    }
    if (story_heading(backend.observe())){
        return "expected no heading where nothing read it";
    }
    return expect_inputs(backend, "stick 45deg 150ms walk, stick -90deg 300ms run, stick 90deg 150ms walk, release 300ms");
}

//  The fake logs each press by the button's name.
std::string buttons_by_name(CancellableScope& scope){
    FakeStoryBackend<ZonePose> backend({}, scope);
    for (const NintendoSwitch::Button button : {
        NintendoSwitch::BUTTON_L, NintendoSwitch::BUTTON_R, NintendoSwitch::BUTTON_PLUS,
        NintendoSwitch::BUTTON_A, NintendoSwitch::BUTTON_B, NintendoSwitch::BUTTON_ZL,
    }){
        backend.press(button, 80ms, 120ms);
    }
    return expect_inputs(backend, "press L, press R, press +, press A, press B, press ZL");
}

//  Gap 3 and 4: a screen machine over observations, its states matching
//  the choice and interact events, ends on its terminal state.
std::vector<ScreenState> choice_states(std::vector<std::string>& acted){
    const auto state = [&acted](std::string name, ScreenEvent event){
        ScreenState result;
        result.name = name;
        result.matches_observation = [event](const ScreenObservation& observation){
            return observation.events.has(event);
        };
        result.act = [&acted, name]{
            acted.push_back(name);
        };
        result.max_visits = 3;
        return result;
    };
    std::vector<ScreenState> states{
        state("choice", ScreenEvent::CHOICE_BOX),
        state("prompt", ScreenEvent::INTERACT_PROMPT),
        state("text", ScreenEvent::TEXT_BOX),
    };
    states[0].terminal = true;
    return states;
}

//  The machine over `events`, one look each, the last repeating, a second
//  apart.
ScreenMachineResult run_on_events(
    const std::vector<ScreenEvent>& events, CancellableScope& scope, Milliseconds timeout,
    std::vector<std::string>& acted, std::vector<std::string>& lines
){
    const std::vector<ScreenState> states = choice_states(acted);
    StepTrace trace("test", [&lines](const std::string& line){
        lines.push_back(line);
    });
    size_t next = 0;
    const WallClock start = current_time();
    const std::function<ScreenObservation()> observe = [&]{
        ScreenObservation observation;
        observation.when = start + std::chrono::seconds(next);
        observation.events.add(events[std::min(next, events.size() - 1)]);
        next++;
        return observation;
    };
    return run_screen_machine_on_observations(observe, scope, states, timeout, trace);
}

std::string observation_machine_ends(CancellableScope& scope){
    std::vector<std::string> acted;
    std::vector<std::string> lines;
    const ScreenMachineResult result = run_on_events(
        {ScreenEvent::INTERACT_PROMPT, ScreenEvent::TEXT_BOX, ScreenEvent::CHOICE_BOX}, scope, 60s, acted, lines
    );
    if (!result.succeeded() || result.terminal != "choice"){
        return std::format("expected the run to end on choice, got \"{}\"", result.failure);
    }
    if (joined(acted) != "prompt, text, choice"){
        return std::format("expected the states to act in order, got {}", joined(acted));
    }
    if (lines.size() != 3 || !lines[0].starts_with("[test] screen 1: prompt at ")){
        return std::format("expected a trace line per screen, got {}", joined(lines));
    }
    return "";
}

std::string observation_machine_fails(CancellableScope& scope){
    std::vector<std::string> acted;
    std::vector<std::string> lines;
    ScreenMachineResult result = run_on_events({ScreenEvent::BATTLE}, scope, 60s, acted, lines);
    if (result.failure != "The screen matches none of the known states."){
        return std::format("expected no state to match, got \"{}\"", result.failure);
    }
    result = run_on_events({ScreenEvent::TEXT_BOX}, scope, 60s, acted, lines);
    if (!result.failure.starts_with("Screen \"text\" came up more than 3 times")){
        return std::format("expected text to come up too often, got \"{}\"", result.failure);
    }
    result = run_on_events({ScreenEvent::TEXT_BOX, ScreenEvent::INTERACT_PROMPT, ScreenEvent::TEXT_BOX}, scope, 1s, acted, lines);
    if (result.failure != "Did not finish within 1s."){
        return std::format("expected the looks' time to run out, got \"{}\"", result.failure);
    }
    return "";
}

//  A look `seconds` after `start` on a uniform frame of `shade`, showing
//  `event`.
Observation<ZonePose> framed_look(WallClock start, int seconds, uint32_t shade, ScreenEvent event){
    Observation<ZonePose> observation;
    observation.when = start + std::chrono::seconds(seconds);
    observation.events.add(event);
    ImageRGB32 image(16, 16);
    image.fill(shade);
    observation.frame = VideoSnapshot(std::move(image), observation.when);
    return observation;
}

//  A fade that waits (still up to 30 s), a box whose step presses A, and
//  the overworld that ends the run, over `looks` on a fake backend.
ScreenMachineResult run_still_states(const std::vector<Observation<ZonePose>>& looks, CancellableScope& scope){
    FakeStoryBackend<ZonePose> backend({.looks = looks}, scope);
    ScreenState fade;
    fade.name = "fade";
    fade.matches_observation = [](const ScreenObservation& observation){
        return observation.events.has(ScreenEvent::FADE_BLACK);
    };
    fade.max_visits = SIZE_MAX;
    fade.max_still = 30s;
    ScreenState text;
    text.name = "text";
    text.matches_observation = [](const ScreenObservation& observation){
        return observation.events.has(ScreenEvent::TEXT_BOX);
    };
    text.act = [&backend]{
        backend.press(NintendoSwitch::BUTTON_A, 80ms, 120ms);
    };
    text.max_visits = SIZE_MAX;
    ScreenState done;
    done.name = "done";
    done.matches_observation = [](const ScreenObservation& observation){
        return observation.events.has(ScreenEvent::CAN_MOVE);
    };
    done.max_visits = SIZE_MAX;
    done.terminal = true;
    const std::vector<ScreenState> states{fade, text, done};
    StepTrace trace("test", [](const std::string&){});
    const std::function<ScreenObservation()> observe = [&backend]() -> ScreenObservation{
        return backend.observe();
    };
    return run_screen_machine_on_observations(observe, scope, states, 600s, trace);
}

//  A waiting state may stay still up to its own limit; a state that
//  presses and changes nothing is a stall after 8 looks.
std::string still_states(CancellableScope& scope){
    const WallClock start = current_time();
    std::vector<Observation<ZonePose>> waits;
    for (int second = 0; second < 12; second++){
        waits.push_back(framed_look(start, second, 0xff000000, ScreenEvent::FADE_BLACK));
    }
    waits.push_back(framed_look(start, 12, 0xff808080, ScreenEvent::CAN_MOVE));
    ScreenMachineResult result = run_still_states(waits, scope);
    if (result.terminal != "done"){
        return std::format("expected a 12 s still fade to end on done, got \"{}\"", result.failure);
    }
    std::vector<Observation<ZonePose>> too_long;
    for (int second = 0; second < 40; second++){
        too_long.push_back(framed_look(start, second, 0xff000000, ScreenEvent::FADE_BLACK));
    }
    result = run_still_states(too_long, scope);
    if (result.failure != "The screen stayed still on \"fade\" for more than 30s."){
        return std::format("expected the fade to stay still too long, got \"{}\"", result.failure);
    }
    std::vector<Observation<ZonePose>> presses;
    for (int second = 0; second < 12; second++){
        presses.push_back(framed_look(start, second, 0xffffffff, ScreenEvent::TEXT_BOX));
    }
    result = run_still_states(presses, scope);
    if (result.failure != "The screen stopped changing: 8 steps on \"text\" did nothing."){
        return std::format("expected the box to stall, got \"{}\"", result.failure);
    }
    return "";
}

//  A reader that logs "[Test] read <name>" when it runs and fills nothing.
StoryGameReader<GridPose> logging_reader(std::string name, std::vector<std::string> supplies, StoryReaderUse use){
    StoryGameReader<GridPose> reader;
    reader.name = name;
    reader.supplies = std::move(supplies);
    reader.use = use;
    reader.read = [name](StoryBackend<GridPose>& backend, Observation<GridPose>&){
        backend.log(std::format("[Test] read {}", name));
    };
    return reader;
}

//  The readers `checkpoint`'s postcondition check runs on a backend of
//  `kind`, as "pose, bag".
std::string readers_run(const StoryCheckpoint& checkpoint, StoryBackendKind kind, CancellableScope& scope){
    StoryGame<GridPose> game;
    game.readers.push_back(logging_reader("pose", {"pose"}, StoryReaderUse::AFTER_CHECKPOINT));
    game.readers.push_back(logging_reader("bag", {"bag.*"}, StoryReaderUse::AFTER_CHECKPOINT));
    game.readers.push_back(logging_reader("heading", {"heading"}, StoryReaderUse::ON_REQUEST));
    StoryGameReader<GridPose> flags = logging_reader("flags", {"flag.*"}, StoryReaderUse::AFTER_CHECKPOINT);
    flags.console_only = true;
    game.readers.push_back(std::move(flags));
    FakeStoryBackend<GridPose> backend({.kind = kind}, scope);
    StoryEngine<GridPose> engine(game, backend);
    engine.check_postconditions(checkpoint);
    std::vector<std::string> read;
    for (const std::string& line : backend.lines()){
        if (line.starts_with("[Test] read ")){
            read.push_back(line.substr(12));
        }
    }
    return joined(read);
}

//  The postcondition check runs only the readers that opted in and supply
//  what it needs, and no console_only one off a console; never the
//  heading, which opted out.
std::string readers_opt_in(CancellableScope& scope){
    StoryCheckpoint items = checkpoint_of("01", {});
    items.postconditions.items = {{.item = "POTION", .quantity = 1}};
    std::string read = readers_run(items, StoryBackendKind::SCRIPT, scope);
    if (read != "bag"){
        return std::format("expected only the bag read for an item, got \"{}\"", read);
    }
    StoryCheckpoint place = checkpoint_of("02", {});
    place.postconditions.position = GridPose{.map = "Town", .tiles = {{1, 2}}, .facing = std::nullopt};
    place.postconditions.flags = {"DOOR"};
    read = readers_run(place, StoryBackendKind::SCRIPT, scope);
    if (read != "pose"){
        return std::format("expected only the pose read off a console, got \"{}\"", read);
    }
    read = readers_run(place, StoryBackendKind::CONSOLE, scope);
    if (read != "pose, flags"){
        return std::format("expected the pose and the console's flags read, got \"{}\"", read);
    }
    read = readers_run(checkpoint_of("03", {}), StoryBackendKind::CONSOLE, scope);
    return read.empty() ? "" : std::format("expected no reader without postconditions, got \"{}\"", read);
}

//  Gap 5: an action sees its step.
std::string handler_sees_step(CancellableScope& scope){
    StoryGame<GridPose> game;
    std::shared_ptr<std::string> seen = std::make_shared<std::string>();
    game.actions.push_back(game_action<GridPose>("scene", [seen](StoryBackend<GridPose>&, const StoryActionCall& call, RunState&){
        *seen = std::format("{} / {}", call.step.name, call.step.why);
        return outcome_ok();
    }));
    const StoryActionRegistry<GridPose> registry(game);
    FakeStoryBackend<GridPose> backend({}, scope);
    RunState state;
    const Outcome outcome = registry.dispatch(backend, game_step("opening", "scene", "the long intro"), state);
    if (!outcome.succeeded()){
        return std::format("expected a pass, got {}", to_string(outcome));
    }
    return *seen == "opening / the long intro" ? "" : std::format("expected the step, got \"{}\"", *seen);
}

//  Gap 6: the save is told its checkpoint, at the end and at a SAVE step.
std::string save_gets_checkpoint(CancellableScope& scope){
    StoryGame<GridPose> game = flaky_game<GridPose>(0);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    StoryStep save;
    save.kind = StoryStepKind::SAVE;
    save.name = "save_here";
    const StoryCheckpoint checkpoint = in_phase(checkpoint_of("02.01", {game_step("walk", "fine"), save}), StoryPhase::REPRODUCTION, engine);
    RunState state;
    const StoryEngineReport report = engine.run_checkpoint(checkpoint, SavePoint::CHECKPOINT_END, state);
    if (!report.saved){
        return "expected the checkpoint to save at its end";
    }
    const size_t saves = count_lines(backend, "[Test] save 02.01");
    return saves == 2 ? "" : std::format("expected 2 saves of 02.01, got {}: {}", saves, joined(backend.lines()));
}

//  Gap 8: a console_only action is refused, before any input, on a backend
//  that is no console; it runs on one.
StoryGame<GridPose> console_game(){
    StoryGame<GridPose> game = flaky_game<GridPose>(0);
    StoryAction<GridPose> menu = game_action<GridPose>("open_menu", [](StoryBackend<GridPose>& backend, const StoryActionCall&, RunState&){
        backend.press(NintendoSwitch::BUTTON_X, 80ms, 120ms);
        return outcome_ok();
    });
    menu.spec.console_only = true;
    game.actions.push_back(std::move(menu));
    return game;
}

std::string console_only_refused(CancellableScope& scope){
    const StoryGame<GridPose> game = console_game();
    const StoryActionRegistry<GridPose> registry(game);
    RunState state;
    FakeStoryBackend<GridPose> script({}, scope);
    const Outcome refused = registry.dispatch(script, game_step("menu", "open_menu"), state);
    if (refused.failure != CONSOLE_ONLY || refused.detail != "open_menu runs only on a console, not on a script backend"){
        return std::format("expected console_only, got {}", to_string(refused));
    }
    const std::string none = expect_inputs(script, "");
    if (!none.empty()){
        return none;
    }
    FakeStoryBackend<GridPose> console({.kind = StoryBackendKind::CONSOLE}, scope);
    if (!registry.dispatch(console, game_step("menu", "open_menu"), state).succeeded()){
        return "expected the action to run on a console";
    }
    return expect_inputs(console, "press X");
}

//  Gap 7 and 8: no reload for a console_only failure, which every attempt
//  would repeat.
std::string segment_no_reload_for_console_only(CancellableScope& scope){
    const StoryGame<GridPose> game = console_game();
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    const std::vector<StoryCheckpoint> checkpoints{checkpoint_of("01", {game_step("menu", "open_menu")})};
    RunState state;
    const SegmentReport report = engine.run_segment("seg", checkpoints, 3, state);
    if (report.completed || report.failure.failure != CONSOLE_ONLY || report.failed_event != "menu"){
        return std::format("expected the segment to stop on console_only at menu, got {}", to_string(report.failure));
    }
    return count_lines(backend, "[Test] reload") == 0 ? "" : "expected no reload";
}

//  Gap 7: the first checkpoint is retried after a reload; one after it
//  only when the one before saved at its end (#329).
struct SegmentInput{
    std::string_view name;
    StoryPhase phase;
    //  Which checkpoint fails once: 0 or 1.
    size_t flaky = 0;
};

struct SegmentExpectation{
    bool completed = false;
    size_t reloads = 0;
    size_t saves = 0;
};

struct SegmentCase{
    SegmentInput input;
    SegmentExpectation expectation;
};

const SegmentCase SEGMENT_CASES[] = {
    {.input = {.name = "first retries while testing", .phase = StoryPhase::TESTING, .flaky = 0},
     .expectation = {.completed = true, .reloads = 1, .saves = 0}},
    {.input = {.name = "second does not retry while testing", .phase = StoryPhase::TESTING, .flaky = 1},
     .expectation = {.completed = false, .reloads = 0, .saves = 0}},
    {.input = {.name = "second does not retry while verifying", .phase = StoryPhase::VERIFICATION, .flaky = 1},
     .expectation = {.completed = false, .reloads = 0, .saves = 0}},
    {.input = {.name = "second retries after a reproduction save", .phase = StoryPhase::REPRODUCTION, .flaky = 1},
     .expectation = {.completed = true, .reloads = 1, .saves = 2}},
};

std::string check_segment(const SegmentCase& test, CancellableScope& scope){
    const StoryGame<GridPose> game = flaky_game<GridPose>(1);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    std::vector<StoryCheckpoint> checkpoints;
    for (size_t c = 0; c < 2; c++){
        const std::string action = c == test.input.flaky ? "flaky" : "fine";
        checkpoints.push_back(in_phase(checkpoint_of(std::format("0{}", c + 1), {game_step("walk", action)}), test.input.phase, engine));
    }
    RunState state;
    const SegmentReport report = engine.run_segment("seg", checkpoints, 3, state);
    const size_t reloads = count_lines(backend, "[Test] reload");
    const size_t saves = count_lines(backend, "[Test] save 01") + count_lines(backend, "[Test] save 02");
    if (report.completed != test.expectation.completed || reloads != test.expectation.reloads || saves != test.expectation.saves){
        return std::format(
            "expected completed {}, {} reloads, {} saves; got {}, {}, {}: {}",
            test.expectation.completed, test.expectation.reloads, test.expectation.saves,
            report.completed, reloads, saves, joined(backend.lines())
        );
    }
    return report.reloads == reloads ? "" : "expected the report to count each reload";
}

class StorySegmentTest : public UnitTest{
public:
    explicit StorySegmentTest(SegmentCase test)
        : UnitTest(std::format("Pokemon::StoryEngine segment - {}", test.input.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = check_segment(m_case, scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    SegmentCase m_case;
};

//  Gap 7: verify runs a checkpoint 3 times from one save, saving only at
//  the end of the last, and only when every run passed.
std::string verify_saves_after_all_passed(CancellableScope& scope){
    for (size_t failures = 0; failures < 2; failures++){
        const StoryGame<GridPose> game = flaky_game<GridPose>(failures);
        FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::CAN_MOVE})}}, scope);
        StoryEngine<GridPose> engine(game, backend);
        const StoryCheckpoint checkpoint = in_phase(checkpoint_of("00.15b", {game_step("battle", "flaky")}), StoryPhase::VERIFICATION, engine);
        RunState state;
        const VerifyReport report = engine.verify_checkpoint(checkpoint, VerifyPolicy{.runs = 3, .passes_needed = 2}, state);
        const size_t saves = count_lines(backend, "[Test] save 00.15b");
        const size_t reloads = count_lines(backend, "[Test] reload");
        const size_t passes = 3 - failures;
        if (!report.verified || report.passes != passes || reloads != 2 || saves != (failures == 0 ? 1 : 0) || report.saved != (failures == 0)){
            return std::format(
                "{} failures: expected verified, {} passes, 2 reloads, {} saves; got {}, {}, {}, {}",
                failures, passes, failures == 0 ? 1 : 0, report.verified, report.passes, reloads, saves
            );
        }
    }
    return "";
}


//  One check above, by name.
struct RunsInput{
    std::string_view name;
    std::string (*check)(CancellableScope& scope);
};

//  The check's error: none.
struct RunsExpectation{
    std::string_view error = {};
};

struct RunsCase{
    RunsInput input;
    RunsExpectation expectation;
};

const RunsCase RUNS_CASES[] = {
    {.input = {.name = "zone checkpoint on a fake backend", .check = &zone_checkpoint_runs}, .expectation = {}},
    {.input = {.name = "zone start with tiles fails the lint", .check = &zone_rejects_tiles}, .expectation = {}},
    {.input = {.name = "tile start with a place fails the lint", .check = &grid_rejects_places}, .expectation = {}},
    {.input = {.name = "zone game rejects tile args", .check = &zone_rejects_tile_args}, .expectation = {}},
    {.input = {.name = "tile actions only with tiles", .check = &tile_actions_only_with_tiles}, .expectation = {}},
    {.input = {.name = "stick and heading", .check = &stick_and_heading}, .expectation = {}},
    {.input = {.name = "buttons by name", .check = &buttons_by_name}, .expectation = {}},
    {.input = {.name = "waiting states may stay still", .check = &still_states}, .expectation = {}},
    {.input = {.name = "readers opt in to postconditions", .check = &readers_opt_in}, .expectation = {}},
    {.input = {.name = "observation machine ends", .check = &observation_machine_ends}, .expectation = {}},
    {.input = {.name = "observation machine fails", .check = &observation_machine_fails}, .expectation = {}},
    {.input = {.name = "handler sees its step", .check = &handler_sees_step}, .expectation = {}},
    {.input = {.name = "save gets its checkpoint", .check = &save_gets_checkpoint}, .expectation = {}},
    {.input = {.name = "console_only refused off console", .check = &console_only_refused}, .expectation = {}},
    {.input = {.name = "no reload for console_only", .check = &segment_no_reload_for_console_only}, .expectation = {}},
    {.input = {.name = "verify saves after all passed", .check = &verify_saves_after_all_passed}, .expectation = {}},
};

class StoryEngineRunsTest : public UnitTest{
public:
    explicit StoryEngineRunsTest(RunsCase test)
        : UnitTest(std::format("Pokemon::StoryEngine runs - {}", test.input.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = m_case.input.check(scope);
        return error == m_case.expectation.error ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    RunsCase m_case;
};


} // namespace


void add_tests_StoryEngineRuns(UnitTestDatabase& database){
    for (const RunsCase& test : RUNS_CASES){
        database.add<StoryEngineRunsTest>(test);
    }
    for (const SegmentCase& test : SEGMENT_CASES){
        database.add<StorySegmentTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
