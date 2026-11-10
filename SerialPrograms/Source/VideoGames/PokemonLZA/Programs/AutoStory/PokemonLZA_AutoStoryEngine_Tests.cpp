/*  PokemonLZA AutoStory - Engine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00-01.01's scene steps and walks, and every checkpoint, through the shared
 *  story engine on its scripted backend (FakeStoryBackend).
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStoryEngine_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Observation;
using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::ZonePose;
using EngineEvent = PokemonAutomation::ScreenEvent;


namespace{

using FakeBackend = Pokemon::FakeStoryBackend<ZonePose>;

//  A scripted PLZA showing `looks` in order (the last repeats); `kind`
//  CONSOLE runs the console_only actions' code.
FakeBackend fake_plza(
    std::vector<Observation<ZonePose>> looks, CancellableScope& scope,
    Pokemon::StoryBackendKind kind = Pokemon::StoryBackendKind::SCRIPT
){
    return FakeBackend(Pokemon::FakeStoryScript<ZonePose>{.looks = std::move(looks), .battle = Pokemon::outcome_ok(), .kind = kind}, scope);
}

//  A look at `at` after a fixed start showing `events`.
Observation<ZonePose> look_at(Milliseconds at, std::initializer_list<EngineEvent> events = {}){
    static const WallClock START = current_time();
    Observation<ZonePose> look;
    look.when = START + at;
    look.events = ScreenEvents(events);
    return look;
}

//  The step `name` of checkpoint `id`, alone in a checkpoint of its own.
StoryCheckpoint step_of(std::string_view id, std::string_view name){
    const auto found = std::find_if(story_checkpoints().begin(), story_checkpoints().end(), [id](const StoryCheckpoint& row){
        return row.id == id;
    });
    if (found == story_checkpoints().end()){
        return StoryCheckpoint{};
    }
    const StoryCheckpoint& row = *found;
    StoryCheckpoint one;
    one.id = row.id;
    one.title = row.title;
    for (const StoryStep& step : row.steps){
        if (step.name == name){
            one.steps.push_back(step);
        }
    }
    return one;
}

//  `checkpoint` on PLZA's engine over `backend`.
Pokemon::StoryEngineReport run_plza(FakeBackend& backend, const StoryCheckpoint& checkpoint){
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game(StarterChoice::CHIKORITA);
    Pokemon::StoryEngine<ZonePose> engine(game, backend);
    Pokemon::RunState state;
    return engine.run_checkpoint(checkpoint, state);
}

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += (text.empty() ? "" : ", ") + line;
    }
    return text;
}

UnitTestResult expect_inputs(const FakeBackend& backend, const std::vector<std::string>& expected){
    if (backend.inputs() != expected){
        return "Expected inputs [" + joined(expected) + "], got [" + joined(backend.inputs()) + "].";
    }
    return true;
}


//  `look` with the reading `name` on screen.
Observation<ZonePose> with_reading(Observation<ZonePose> look, std::string_view name){
    look.readings[std::string(name)] = 1;
    return look;
}

//  The opening (opening_to_platform): no presses through the cutscene and
//  its skip prompt; the platform before the autosave does not count; done
//  on the second look in a row at the platform after the autosave badge.
UnitTestResult test_opening_waits_for_platform(CancellableScope& scope){
    FakeBackend backend = fake_plza({
        look_at(0ms, {EngineEvent::FADE_BLACK}),
        with_reading(look_at(300ms), SKIP_PROMPT_READING),
        with_reading(look_at(600ms), PLATFORM_READING),
        with_reading(look_at(900ms), PLATFORM_READING),
        with_reading(with_reading(look_at(1200ms), PLATFORM_READING), AUTOSAVE_READING),
        with_reading(look_at(1500ms), PLATFORM_READING),
        with_reading(look_at(1800ms), PLATFORM_READING),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.00", "opening"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return "Expected the opening to end at the platform: " + to_string(report.run.failure);
    }
    const auto logged = [&backend](std::string_view text){
        return std::any_of(backend.lines().begin(), backend.lines().end(), [text](const std::string& line){
            return line.find(text) != std::string::npos;
        });
    };
    if (!logged("skip prompt") || !logged("autosave")){
        return "Expected the skip prompt and the autosave in the log.";
    }
    return expect_inputs(backend, std::vector<std::string>(6, "wait 300ms"));
}

//  01.01's start check: one look at the platform is not enough, and with
//  no platform it fails within its 10 s, pressing nothing.
UnitTestResult test_start_check_needs_platform(CancellableScope& scope){
    FakeBackend backend = fake_plza({
        with_reading(look_at(0ms), PLATFORM_READING),
        look_at(500ms),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "on_platform"));
    if (report.run.completed || report.run.failure.failure != "platform_not_seen"){
        return "Expected platform_not_seen, got " + to_string(report.run.failure);
    }
    for (const std::string& input : backend.inputs()){
        if (input != "wait 300ms"){
            return "Expected only waits, got " + input;
        }
    }
    return true;
}

//  A walk starts only on free control: platform_to_plaza on a text box
//  fails before any input.
UnitTestResult test_walk_needs_free_control(CancellableScope& scope){
    FakeBackend backend = fake_plza({look_at(0ms, {EngineEvent::TEXT_BOX})}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "platform_to_plaza"));
    if (report.run.completed || report.run.failure.failure != "walk_not_free"){
        return "Expected walk_not_free, got " + to_string(report.run.failure);
    }
    return expect_inputs(backend, {});
}

//  platform_to_plaza: the first leg, the turn (L, the stick tapped at 90
//  degrees) and L again, then the last leg held in slices until the white
//  fade.
UnitTestResult test_walk_ends_on_white_fade(CancellableScope& scope){
    FakeBackend backend = fake_plza({
        look_at(0ms), look_at(150ms),
        look_at(12000ms), look_at(12250ms), look_at(12500ms, {EngineEvent::FADE_WHITE}),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "platform_to_plaza"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return "Expected the walk to end on the white fade: " + to_string(report.run.failure);
    }
    return expect_inputs(backend, {
        "wait 150ms", "hold north 5500ms walk", "press L", "stick 90deg 150ms walk, release 300ms", "press L",
        "hold north 250ms walk", "hold north 250ms walk", "hold north 250ms walk",
    });
}

//  platform_to_plaza with no white fade fails once the last leg's 6 s are
//  held: 24 slices.
UnitTestResult test_walk_fails_at_bound(CancellableScope& scope){
    FakeBackend backend = fake_plza({look_at(0ms)}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "platform_to_plaza"));
    if (report.run.completed || report.run.failure.failure != "walk_no_end"){
        return "Expected walk_no_end, got " + to_string(report.run.failure);
    }
    const size_t slices = (size_t)std::count(backend.inputs().begin(), backend.inputs().end(), "hold north 250ms walk");
    if (slices != 24){
        return std::format("Expected 24 slices of 250 ms, held {}.", slices);
    }
    return true;
}

//  A look at `at` facing `heading`, as the minimap reads it.
Observation<ZonePose> facing(Milliseconds at, int heading){
    Observation<ZonePose> look = look_at(at);
    look.readings[std::string(Pokemon::STORY_HEADING)] = heading;
    return look;
}

//  face_heading() turns by the difference to the heading a look reads and
//  stops once a look reads it within the tolerance; with no heading read it
//  fails before any input.
UnitTestResult test_face_heading(CancellableScope& scope){
    FakeBackend backend = fake_plza({facing(0ms, 0), facing(500ms, 88)}, scope);
    const Pokemon::Outcome faced = face_heading(backend, PlzaReader(), 90, 10);
    if (!faced.succeeded()){
        return "Expected to face 90 degrees: " + to_string(faced);
    }
    const UnitTestResult turned = expect_inputs(backend, {"press L", "stick 90deg 150ms walk, release 300ms"});
    if (turned.result != UnitTestResult::PASSED){
        return turned;
    }
    FakeBackend blind = fake_plza({look_at(0ms)}, scope);
    const Pokemon::Outcome unread = face_heading(blind, PlzaReader(), 90, 10);
    if (unread.failure != "heading_unread"){
        return "Expected heading_unread with no heading read, got " + to_string(unread);
    }
    return expect_inputs(blind, {});
}

//  Pancham cornered (text_to_choice): B through the text, done at the
//  choice without pressing it.
UnitTestResult test_text_to_choice(CancellableScope& scope){
    const Observation<ZonePose> choice = look_at(2000ms, {EngineEvent::TEXT_BOX, EngineEvent::CHOICE_BOX});
    FakeBackend backend = fake_plza({look_at(0ms, {EngineEvent::TEXT_BOX}), choice}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "pancham_cornered"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return "Expected the scene to end at the choice: " + to_string(report.run.failure);
    }
    return expect_inputs(backend, {"press B"});
}

//  What PLZA refuses: console actions fail as console_only off a console,
//  and tile walks and unknown actions fail the lint.
UnitTestResult test_refused_steps(CancellableScope& scope){
    StoryCheckpoint legs;
    legs.id = "probe";
    StoryStep step;
    step.kind = Pokemon::StoryStepKind::LEGS;
    step.name = "legs";
    step.args["legs"] = std::vector<Pokemon::StoryLeg>{{.direction = Pokemon::Direction::NORTH, .tiles = 1}};
    legs.steps.push_back(step);
    if (plza_story_lint(legs).empty()){
        return "Expected a tile walk to fail the lint.";
    }
    FakeBackend backend = fake_plza({look_at(0ms)}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.00", "language"));
    if (report.run.failure.failure != "console_only"){
        return "Expected the new game's language to need a console, got " + to_string(report.run.failure);
    }
    const std::vector<std::pair<Pokemon::StoryStepKind, std::string>> unknown{
        {Pokemon::StoryStepKind::MASH, "x"},
        {Pokemon::StoryStepKind::GAME, "no_such_move"},
        {Pokemon::StoryStepKind::GAME, "choose_starter"},
        {Pokemon::StoryStepKind::GAME, "choose_2"},
    };
    for (const auto& [kind, action] : unknown){
        StoryCheckpoint probe;
        probe.id = "probe";
        StoryStep bad;
        bad.kind = kind;
        bad.name = "probe";
        bad.action = action;
        probe.steps.push_back(bad);
        if (plza_story_lint(probe).empty()){
            return std::format("Expected {} \"{}\" to fail the lint.", Pokemon::story_step_kind_name(kind), action);
        }
    }
    return expect_inputs(backend, {});
}

//  Every checkpoint through the engine with each action recording itself:
//  its steps in order, each by its action, and no save while testing. The
//  fake says it is a console, so the console_only actions run too.
UnitTestResult test_every_checkpoint_runs(CancellableScope& scope){
    FakeBackend backend = fake_plza({look_at(0ms)}, scope, Pokemon::StoryBackendKind::CONSOLE);
    std::vector<std::string> trace;
    Pokemon::StoryGame<ZonePose> game;
    game.id = "plza";
    for (const Pokemon::StoryActionSpec& spec : plza_story_action_specs()){
        const std::string label = std::format("{}/{}", Pokemon::story_step_kind_name(spec.kind), spec.name);
        game.actions.push_back({.spec = spec, .handler = [&trace, label](Pokemon::StoryBackend<ZonePose>&, Pokemon::RunState&, const Pokemon::StoryActionCall&){
            trace.push_back(label);
            return Pokemon::outcome_ok();
        }});
    }
    size_t saves = 0;
    game.save = [&saves](Pokemon::StoryBackend<ZonePose>&, const StoryCheckpoint&){
        saves++;
        return Pokemon::outcome_ok();
    };
    Pokemon::StoryEngine<ZonePose> engine(game, backend);
    std::vector<std::string> expected;
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
            const Pokemon::StoryActionSpec* spec = Pokemon::find_story_action(plza_story_specs(), step.kind, step.action);
            expected.push_back(std::format("{}/{}", Pokemon::story_step_kind_name(step.kind), spec ? spec->name : "?"));
        }
        Pokemon::RunState state;
        const Pokemon::StoryEngineReport report = engine.run_checkpoint(checkpoint, state);
        if (report.verdict != Pokemon::CheckpointVerdict::PASSED || report.saved){
            return std::format("{}: expected a pass with no save, got {} ({}).", checkpoint.id,
                report.verdict == Pokemon::CheckpointVerdict::PASSED ? "a save" : "a failure", to_string(report.run.failure));
        }
    }
    if (trace != expected){
        return std::format("Expected {} steps in order, ran {}.", expected.size(), trace.size());
    }
    if (saves != 0){
        return std::format("Expected no save from testing checkpoints, saw {}.", saves);
    }
    return true;
}


struct EngineTestInput{
    std::string name;
    std::function<UnitTestResult(CancellableScope&)> body;
};

struct EngineTestExpectation{
};

struct EngineTestCase{
    EngineTestInput input;
    EngineTestExpectation expectation;
};

class Test_AutoStoryEngine : public UnitTest{
public:
    explicit Test_AutoStoryEngine(EngineTestCase test_case)
        : UnitTest("PokemonLZA::AutoStoryEngine - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        try{
            return m_case.input.body(scope);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    EngineTestCase m_case;
};

} // namespace


const EngineTestCase ENGINE_TEST_CASES[] = {
    {.input = {"01.00 opening: no presses, done on two looks at the platform after the autosave", test_opening_waits_for_platform}, .expectation = {}},
    {.input = {"01.01 start check: fails without the platform", test_start_check_needs_platform}, .expectation = {}},
    {.input = {"01.01 text_to_choice: B through the text, done at the choice", test_text_to_choice}, .expectation = {}},
    {.input = {"face_heading: turns by the minimap's heading", test_face_heading}, .expectation = {}},
    {.input = {"01.01 platform_to_plaza: no walk without free control", test_walk_needs_free_control}, .expectation = {}},
    {.input = {"01.01 platform_to_plaza: ends on the white fade", test_walk_ends_on_white_fade}, .expectation = {}},
    {.input = {"01.01 platform_to_plaza: fails at its bound", test_walk_fails_at_bound}, .expectation = {}},
    {.input = {"refused: console actions off a console, tile walks, unknown actions", test_refused_steps}, .expectation = {}},
    {.input = {"every checkpoint runs in step order, testing never saves", test_every_checkpoint_runs}, .expectation = {}},
};

void add_tests_AutoStoryEngine(UnitTestDatabase& database){
    for (const EngineTestCase& item : ENGINE_TEST_CASES){
        database.add<Test_AutoStoryEngine>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
