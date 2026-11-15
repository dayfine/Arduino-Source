/*  Story Engine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The engine's lower layers on a scripted backend: the action registry
 *  (lint and dispatch, a game's action replacing a built-in), the step
 *  runner (skip_if from flags and readers, the declared end pose, the
 *  "[Step]" line), the worked example of a game action, take_warp, and the
 *  scripted backend's inputs.
 *
 */

#include <chrono>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon_StoryActionRegistry.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryStepRunner.h"
#include "Pokemon_StoryEngine_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

using namespace std::chrono_literals;


//  One look: `events`, standing on `pose` if given.
Observation<GridPose> look(std::initializer_list<ScreenEvent> events, std::optional<GridPose> pose = std::nullopt){
    Observation<GridPose> observation;
    observation.events = ScreenEvents(events);
    observation.pose = std::move(pose);
    return observation;
}

GridPose on_map(std::string map){
    return GridPose{.map = std::move(map), .tiles = {}, .facing = std::nullopt};
}

std::string joined(const std::vector<std::string>& items){
    std::string text;
    for (const std::string& item : items){
        text += (text.empty() ? "" : ", ") + item;
    }
    return text;
}

bool has_line(const FakeStoryBackend<GridPose>& backend, std::string_view line){
    for (const std::string& each : backend.lines()){
        if (each == line){
            return true;
        }
    }
    return false;
}


//  The worked example of a game's own action: a door or stairs warp. A
//  warp is a fade out, then a fade in on the target's map. No fade is not
//  "arrived": a run that takes it so goes on, and saves, on the wrong floor.
Outcome take_warp(StoryBackend<GridPose>& backend, const StoryActionCall& call, RunState&){
    const Direction direction = story_arg<Direction>(call.args, "direction");
    const GridPose& to = story_arg<GridPose>(call.args, "to");
    backend.hold(direction, 400ms, StoryPace::WALK);
    bool faded = false;
    for (size_t c = 0; c < 40; c++){
        const Observation<GridPose> now = backend.observe();
        if (now.events.has(ScreenEvent::FADE_BLACK)){
            faded = true;
        }else if (faded && now.pose && now.pose->map == to.map){
            return outcome_ok(pose_of(*now.pose));
        }
        backend.wait(100ms);
    }
    return outcome_failed(faded ? "warp_wrong_map" : "warp_no_fade");
}

//  A game with take_warp, a route of its own (one hold north) and a
//  reader "door_open" that reads `door_open`.
StoryGame<GridPose> test_game(std::optional<int> door_open = std::nullopt){
    StoryGame<GridPose> game;
    game.id = "test";
    game.actions.push_back({
        .spec = {.kind = StoryStepKind::GAME, .name = "take_warp", .args = {
            {.key = "direction", .type = StoryArgType::DIRECTION, .required = true, .fallback = std::nullopt, .why = "into the door"},
            {.key = "to", .type = StoryArgType::GRID_POSE, .required = true, .fallback = std::nullopt, .why = "the warp's end"},
        }},
        .handler = &take_warp,
    });
    game.actions.push_back({
        .spec = *find_story_action(builtin_story_actions(), StoryStepKind::ROUTE, "route"),
        .handler = [](StoryBackend<GridPose>& backend, const StoryActionCall&, RunState&){
            backend.hold(Direction::NORTH, 100ms, StoryPace::RUN);
            return outcome_ok();
        },
    });
    if (door_open){
        game.readers.push_back({
            .name = "door_open", .supplies = {"door_open"}, .use = StoryReaderUse::ON_REQUEST, .console_only = false,
            .read = [value = *door_open](StoryBackend<GridPose>&, Observation<GridPose>& observation){
                observation.readings["door_open"] = value;
            },
        });
    }
    return game;
}

StoryStep make_step(StoryStepKind kind, std::string action, StoryActionArgs args){
    StoryStep step;
    step.kind = kind;
    step.name = "s";
    step.action = std::move(action);
    step.args = std::move(args);
    return step;
}



struct RegistryCase{
    //  A step checked and, when it lints clean, dispatched on a backend that
    //  shows talk text once, then the overworld.
    struct Input{
        StoryStep step;
    };

    //  The lint errors joined by "; ", then (for a clean step) the failure and
    //  the inputs sent.
    struct Expectation{
        std::string_view lint = {};
        std::string_view failure = {};
        std::string_view inputs = {};
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const RegistryCase REGISTRY_CASES[] = {
    {.name = "legs tap each tile", .input = {.step = make_step(StoryStepKind::LEGS, "", {{"legs", std::vector<StoryLeg>{{Direction::NORTH, 2}, {Direction::EAST, 1}}}})},
     .expectation = {.inputs = "tap north, tap north, tap east"}},
    {.name = "legs empty", .input = {.step = make_step(StoryStepKind::LEGS, "", {{"legs", std::vector<StoryLeg>{}}})},
     .expectation = {.lint = "no legs"}},
    {.name = "face without direction", .input = {.step = make_step(StoryStepKind::STEP, "face", {})},
     .expectation = {.lint = "missing argument \"direction\" (the way to step or face)"}},
    {.name = "unknown game action", .input = {.step = make_step(StoryStepKind::GAME, "take_wrap", {})},
     .expectation = {.lint = "unknown action \"take_wrap\" for GAME"}},
    {.name = "talk", .input = {.step = make_step(StoryStepKind::TALK, "", {})},
     .expectation = {.inputs = "press A, press A"}},
    {.name = "answer no", .input = {.step = make_step(StoryStepKind::ANSWER, "", {{"yes", false}})},
     .expectation = {.inputs = "press B"}},
    {.name = "battle", .input = {.step = make_step(StoryStepKind::BATTLE, "", {})},
     .expectation = {.inputs = "battle"}},
    {.name = "ready needs the game", .input = {.step = make_step(StoryStepKind::READY, "", {{"target", on_map("CeruleanCity")}})},
     .expectation = {.failure = "unsupported_step"}},
    {.name = "game route replaces the built-in", .input = {.step = make_step(StoryStepKind::ROUTE, "", {{"target", on_map("Route4")}})},
     .expectation = {.inputs = "hold north 100ms run"}},
    {.name = "save without the game's save", .input = {.step = make_step(StoryStepKind::SAVE, "", {})},
     .expectation = {.failure = "unsupported_step"}},
};

std::string check_registry(const RegistryCase& test, CancellableScope& scope){
    const StoryGame<GridPose> game = test_game();
    const StoryActionRegistry<GridPose> registry(game);
    std::vector<std::string> lint = registry.check(test.input.step);
    if (joined(lint) != test.expectation.lint){
        return std::format("expected lint \"{}\", got \"{}\"", test.expectation.lint, joined(lint));
    }
    if (!lint.empty()){
        return "";
    }
    FakeStoryBackend<GridPose> backend({.looks = {look({ScreenEvent::DIALOG_BOX}), look({ScreenEvent::DIALOG_BOX}), look({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    RunState state;
    const Outcome outcome = registry.dispatch(backend, test.input.step, state);
    if (outcome.failure != test.expectation.failure){
        return std::format("expected failure \"{}\", got \"{}\"", test.expectation.failure, to_string(outcome));
    }
    if (joined(backend.inputs()) != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, joined(backend.inputs()));
    }
    return "";
}

class StoryActionRegistryTest : public UnitTest{
public:
    explicit StoryActionRegistryTest(RegistryCase test)
        : UnitTest(std::format("Pokemon::StoryEngine registry - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = check_registry(m_case, scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    RegistryCase m_case;
};



struct RunnerCase{
    //  One step, read from JSON, run by the step runner on a backend that
    //  shows the overworld, with `flags` set and the "door_open" reader
    //  reading `door_open`.
    struct Input{
        std::string_view step;
        std::vector<std::string> flags = {};
        std::optional<int> door_open = std::nullopt;
    };

    //  The failure, a line the run logged and the map of the outcome's pose.
    struct Expectation{
        std::string_view failure = {};
        std::string_view line = {};
        std::string_view pose_map = {};
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const RunnerCase RUNNER_CASES[] = {
    {.name = "skip_if a flag", .input = {.step = R"({"id": "s", "kind": "talk", "skip_if": ["got_candy"]})", .flags = {"got_candy"}},
     .expectation = {.line = "[Step] s TALK/default: skipped"}},
    {.name = "skip_if a reader", .input = {.step = R"({"id": "s", "kind": "talk", "skip_if": ["door_open"]})", .door_open = 1},
     .expectation = {.line = "[Step] s TALK/default: skipped"}},
    {.name = "skip_if a reader that says no", .input = {.step = R"({"id": "s", "kind": "scene", "skip_if": ["door_open"]})", .door_open = 0},
     .expectation = {.line = "[Step] s MASH/default: ok"}},
    {.name = "skip_if nobody answers", .input = {.step = R"({"id": "s", "kind": "scene", "skip_if": ["door_open"]})"},
     .expectation = {.line = "[Step] s MASH/default: ok"}},
    {.name = "declared end pose", .input = {.step = R"({"id": "s", "kind": "scene", "declared_after": {"map": "Route1", "tiles": [[1, 2]]}})"},
     .expectation = {.line = "[Step] s MASH/default: ok", .pose_map = "Route1"}},
    {.name = "talk to nobody fails", .input = {.step = R"({"id": "s", "kind": "talk"})"},
     .expectation = {.failure = "talk_no_text", .line = "[Step] s TALK/default: failed: talk_no_text: A opened no text box (class=screen_unread)"}},
};

std::string check_runner(const RunnerCase& test, CancellableScope& scope){
    const StoryGame<GridPose> game = test_game(test.input.door_open);
    const StoryActionRegistry<GridPose> registry(game);
    FakeStoryBackend<GridPose> backend({.looks = {look({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryStepRunner<GridPose> runner(registry, backend);
    RunState state;
    state.flags.insert(test.input.flags.begin(), test.input.flags.end());
    const StoryStep step = read_story_step(parse_json(std::string(test.input.step)), test.name, registry.specs());
    const Outcome outcome = runner.run(step, state);
    if (outcome.failure != test.expectation.failure){
        return std::format("expected failure \"{}\", got \"{}\"", test.expectation.failure, to_string(outcome));
    }
    if (!has_line(backend, test.expectation.line)){
        return std::format("expected the line \"{}\", logged: {}", test.expectation.line, joined(backend.lines()));
    }
    const std::string map = outcome.pose ? outcome.pose->map : "";
    if (map != test.expectation.pose_map){
        return std::format("expected the pose on \"{}\", got \"{}\"", test.expectation.pose_map, map);
    }
    return "";
}

class StoryStepRunnerTest : public UnitTest{
public:
    explicit StoryStepRunnerTest(RunnerCase test)
        : UnitTest(std::format("Pokemon::StoryEngine runner - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = check_runner(m_case, scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    RunnerCase m_case;
};



struct WarpCase{
    //  take_warp north to B3F, from B4F, on a scripted game.
    struct Input{
        std::vector<Observation<GridPose>> looks;
    };

    //  How take_warp ended: "" for arrived.
    struct Expectation{
        std::string_view failure = {};
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const WarpCase WARP_CASES[] = {
    {.name = "fade then B3F", .input = { .looks = {
        look({}, on_map("RocketHideout_B4F")), look({ScreenEvent::FADE_BLACK}), look({ScreenEvent::PLAYER_CAN_MOVE}, on_map("RocketHideout_B3F")),
     }}, .expectation = {}},
    //  The screen darkens at the stairs but no fade starts and the map
    //  stays B4F: a wait for a dark screen alone would take this for arrived.
    {.name = "black_without_warp", .input = {.looks = {look({ScreenEvent::SCREEN_STILL}, on_map("RocketHideout_B4F"))}},
     .expectation = {.failure = "warp_no_fade"}},
    {.name = "fade back to B4F", .input = {.looks = {look({ScreenEvent::FADE_BLACK}), look({ScreenEvent::PLAYER_CAN_MOVE}, on_map("RocketHideout_B4F"))}},
     .expectation = {.failure = "warp_wrong_map"}},
};

std::string check_warp(const WarpCase& test, CancellableScope& scope){
    const StoryGame<GridPose> game = test_game();
    const StoryActionRegistry<GridPose> registry(game);
    FakeStoryBackend<GridPose> backend({.looks = test.input.looks}, scope);
    StoryStepRunner<GridPose> runner(registry, backend);
    RunState state;
    const StoryStep step = make_step(StoryStepKind::GAME, "take_warp", {
        {"direction", Direction::NORTH},
        {"to", on_map("RocketHideout_B3F")},
    });
    const Outcome outcome = runner.run(step, state);
    if (outcome.failure != test.expectation.failure){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.failure, to_string(outcome));
    }
    if (outcome.succeeded() && (!outcome.pose || outcome.pose->map != "RocketHideout_B3F")){
        return "expected the pose on RocketHideout_B3F";
    }
    return "";
}

class StoryTakeWarpTest : public UnitTest{
public:
    explicit StoryTakeWarpTest(WarpCase test)
        : UnitTest(std::format("Pokemon::StoryEngine take_warp - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = check_warp(m_case, scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    WarpCase m_case;
};



//  The move stick at an angle, and the heading reading; a turn lets
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
    const std::string expected = "stick 45deg 150ms walk, stick -90deg 300ms run, stick 90deg 150ms walk, release 300ms";
    const std::string got = joined(backend.inputs());
    return got == expected ? "" : std::format("expected inputs \"{}\", got \"{}\"", expected, got);
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
    const std::string expected = "press L, press R, press +, press A, press B, press ZL";
    const std::string got = joined(backend.inputs());
    return got == expected ? "" : std::format("expected inputs \"{}\", got \"{}\"", expected, got);
}

//  An action sees its step.
std::string handler_sees_step(CancellableScope& scope){
    StoryGame<GridPose> game;
    std::shared_ptr<std::string> seen = std::make_shared<std::string>();
    game.actions.push_back({
        .spec = {.kind = StoryStepKind::GAME, .name = "scene", .args = {}},
        .handler = [seen](StoryBackend<GridPose>&, const StoryActionCall& call, RunState&){
            *seen = std::format("{} / {}", call.step.name, call.step.why);
            return outcome_ok();
        },
    });
    const StoryActionRegistry<GridPose> registry(game);
    FakeStoryBackend<GridPose> backend({}, scope);
    StoryStep step = make_step(StoryStepKind::GAME, "scene", {});
    step.name = "opening";
    step.why = "the long intro";
    RunState state;
    const Outcome outcome = registry.dispatch(backend, step, state);
    if (!outcome.succeeded()){
        return std::format("expected a pass, got {}", to_string(outcome));
    }
    return *seen == "opening / the long intro" ? "" : std::format("expected the step, got \"{}\"", *seen);
}

//  A named test; it returns its error, empty when it passed.
struct NamedCheck{
    std::string_view name;
    std::string (*check)(CancellableScope& scope);
};

const NamedCheck NAMED_CHECKS[] = {
    {.name = "stick and heading", .check = &stick_and_heading},
    {.name = "buttons by name", .check = &buttons_by_name},
    {.name = "handler sees its step", .check = &handler_sees_step},
};

class StoryEngineCheckTest : public UnitTest{
public:
    explicit StoryEngineCheckTest(NamedCheck test)
        : UnitTest(std::format("Pokemon::StoryEngine - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = m_case.check(scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    NamedCheck m_case;
};


} // namespace


void add_tests_StoryEngine(UnitTestDatabase& database){
    for (const RegistryCase& test : REGISTRY_CASES){
        database.add<StoryActionRegistryTest>(test);
    }
    for (const RunnerCase& test : RUNNER_CASES){
        database.add<StoryStepRunnerTest>(test);
    }
    for (const WarpCase& test : WARP_CASES){
        database.add<StoryTakeWarpTest>(test);
    }
    for (const NamedCheck& test : NAMED_CHECKS){
        database.add<StoryEngineCheckTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
