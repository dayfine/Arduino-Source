/*  PokemonLZA AutoStory - Engine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00's scene steps and walks, and every checkpoint, through the shared
 *  story engine on a scripted PLZA backend.
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
#include "PokemonLZA_AutoStoryGame.h"
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

//  A scripted PLZA: each observe() returns the script's next look (the
//  last repeats); every input is written down.
//  code-health: allow public_methods -- the backend calls it fakes plus the recording tests read
class FakePlzaBackend : public PlzaStoryBackend{
public:
    FakePlzaBackend(std::vector<Observation<ZonePose>> looks, CancellableScope& scope)
        : m_looks(std::move(looks))
        , m_scope(scope)
    {}

    //  "press A", "wait 250ms", "hold north 250ms", "turn 90".
    const std::vector<std::string>& inputs() const{
        return m_inputs;
    }

    virtual void press(Button button, Milliseconds, Milliseconds) override{
        m_inputs.push_back(std::string("press ") + (button == BUTTON_A ? "A" : button == BUTTON_B ? "B" : button == BUTTON_L ? "L" : "other"));
    }
    virtual void tap(Pokemon::Direction direction) override{
        m_inputs.push_back("tap " + Pokemon::direction_name(direction));
    }
    virtual void hold(Pokemon::Direction direction, Milliseconds duration, Pokemon::StoryPace) override{
        m_inputs.push_back(std::format("hold {} {}ms", Pokemon::direction_name(direction), duration.count()));
    }
    virtual void wait(Milliseconds duration) override{
        m_inputs.push_back(std::format("wait {}ms", duration.count()));
    }
    virtual Observation<ZonePose> observe() override{
        if (m_looks.empty()){
            return {};
        }
        const Observation<ZonePose>& look = m_looks[std::min(m_next, m_looks.size() - 1)];
        m_next++;
        return look;
    }
    virtual Pokemon::Outcome battle(const Pokemon::StoryActionArgs&) override{
        m_inputs.emplace_back("battle");
        return Pokemon::outcome_ok();
    }
    virtual void log(std::string_view) override{}
    virtual CancellableScope& scope() override{
        return m_scope;
    }
    virtual void turn(double degrees) override{
        m_inputs.push_back(std::format("turn {}", degrees));
    }
    virtual double heading() override{
        return 0;
    }

private:
    std::vector<Observation<ZonePose>> m_looks;
    CancellableScope& m_scope;
    size_t m_next = 0;
    std::vector<std::string> m_inputs;
};

//  A look at `at` after a fixed start showing `events`.
Observation<ZonePose> look_at(Milliseconds at, std::initializer_list<EngineEvent> events = {}){
    static const WallClock START = current_time();
    Observation<ZonePose> look;
    look.when = START + at;
    look.events = ScreenEvents(events);
    return look;
}

//  01.00's step `name`, alone in a checkpoint of its own.
StoryCheckpoint step_of_01_00(std::string_view name){
    const StoryCheckpoint& row = story_checkpoints().front();
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
Pokemon::StoryEngineReport run_plza(FakePlzaBackend& backend, const StoryCheckpoint& checkpoint){
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game(backend, StarterChoice::CHIKORITA);
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

UnitTestResult expect_inputs(const FakePlzaBackend& backend, const std::vector<std::string>& expected){
    if (backend.inputs() != expected){
        return "Expected inputs [" + joined(expected) + "], got [" + joined(backend.inputs()) + "].";
    }
    return true;
}


//  The opening (mash_to_move_hints): A at a dialog, a fade waited out with
//  no press, then the quiet end once the scene's 60 s have passed.
UnitTestResult test_opening_scene(CancellableScope& scope){
    FakePlzaBackend backend({
        look_at(0ms, {EngineEvent::TEXT_BOX}),
        look_at(1000ms, {EngineEvent::FADE_BLACK}),
        look_at(2000ms),
        look_at(70000ms, {EngineEvent::STILL}),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of_01_00("opening"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return "Expected the opening to end on its quiet end: " + to_string(report.run.failure);
    }
    return expect_inputs(backend, {"press A", "wait 250ms"});
}

//  The opening does not end on a still screen before its minimum length:
//  it waits, then ends once 60 s have passed.
UnitTestResult test_opening_waits_min_length(CancellableScope& scope){
    FakePlzaBackend backend({
        look_at(0ms, {EngineEvent::STILL}),
        look_at(30000ms, {EngineEvent::STILL}),
        look_at(60000ms, {EngineEvent::STILL}),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of_01_00("opening"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return "Expected the quiet end at 60 s: " + to_string(report.run.failure);
    }
    //  The still look at 0 s polls until the look at 60 s; then it ends.
    return expect_inputs(backend, {"wait 250ms", "wait 250ms"});
}

//  A walk starts only on free control: platform_to_plaza on a text box
//  fails before any input.
UnitTestResult test_walk_needs_free_control(CancellableScope& scope){
    FakePlzaBackend backend({look_at(0ms, {EngineEvent::TEXT_BOX})}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of_01_00("platform_to_plaza"));
    if (report.run.completed || report.run.failure.failure != "walk_not_free"){
        return "Expected walk_not_free, got " + to_string(report.run.failure);
    }
    return expect_inputs(backend, {});
}

//  platform_to_plaza: the first leg, the turn, then the last leg held in
//  slices until the white fade.
UnitTestResult test_walk_ends_on_white_fade(CancellableScope& scope){
    FakePlzaBackend backend({
        look_at(0ms), look_at(150ms),
        look_at(12000ms), look_at(12250ms), look_at(12500ms, {EngineEvent::FADE_WHITE}),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of_01_00("platform_to_plaza"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return "Expected the walk to end on the white fade: " + to_string(report.run.failure);
    }
    return expect_inputs(backend, {
        "wait 150ms", "hold north 5500ms", "turn 90", "press L",
        "hold north 250ms", "hold north 250ms", "hold north 250ms",
    });
}

//  platform_to_plaza with no white fade fails once the last leg's 6 s are
//  held: 24 slices.
UnitTestResult test_walk_fails_at_bound(CancellableScope& scope){
    FakePlzaBackend backend({look_at(0ms)}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of_01_00("platform_to_plaza"));
    if (report.run.completed || report.run.failure.failure != "walk_no_end"){
        return "Expected walk_no_end, got " + to_string(report.run.failure);
    }
    const size_t slices = (size_t)std::count(backend.inputs().begin(), backend.inputs().end(), "hold north 250ms");
    if (slices != 24){
        return std::format("Expected 24 slices of 250 ms, held {}.", slices);
    }
    return true;
}

//  Pancham cornered (text_to_choice): B through the text, done at the
//  choice without pressing it.
UnitTestResult test_text_to_choice(CancellableScope& scope){
    Observation<ZonePose> choice = look_at(2000ms, {EngineEvent::TEXT_BOX});
    choice.readings[std::string(CHOICE_BOX_READING)] = 1;
    FakePlzaBackend backend({look_at(0ms, {EngineEvent::TEXT_BOX}), choice}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of_01_00("pancham_cornered"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return "Expected the scene to end at the choice: " + to_string(report.run.failure);
    }
    return expect_inputs(backend, {"press B"});
}

//  What PLZA refuses: tile walks fail by name, console actions fail as
//  console_only off a console, and unknown actions fail the lint.
UnitTestResult test_refused_steps(CancellableScope& scope){
    StoryCheckpoint legs;
    legs.id = "probe";
    StoryStep step;
    step.kind = Pokemon::StoryStepKind::LEGS;
    step.name = "legs";
    step.args["legs"] = std::vector<Pokemon::StoryLeg>{{.direction = Pokemon::Direction::NORTH, .tiles = 1}};
    legs.steps.push_back(step);
    FakePlzaBackend backend({look_at(0ms)}, scope);
    Pokemon::StoryEngineReport report = run_plza(backend, legs);
    if (report.run.failure.failure != "unsupported_step"){
        return "Expected a tile walk to fail as unsupported_step, got " + to_string(report.run.failure);
    }
    report = run_plza(backend, step_of_01_00("language"));
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
//  its steps in order, each by its action, and no save while testing.
UnitTestResult test_every_checkpoint_runs(CancellableScope& scope){
    FakePlzaBackend backend({look_at(0ms)}, scope);
    std::vector<std::string> trace;
    Pokemon::StoryGame<ZonePose> game;
    game.id = "plza";
    for (const Pokemon::StoryActionSpec& spec : plza_story_action_specs()){
        const std::string label = std::format("{}/{}", Pokemon::story_step_kind_name(spec.kind), spec.name);
        game.actions.push_back({.spec = spec, .handler = [&trace, label](Pokemon::StoryBackend<ZonePose>&, Pokemon::RunState&, const Pokemon::StoryActionArgs&){
            trace.push_back(label);
            return Pokemon::outcome_ok();
        }});
    }
    size_t saves = 0;
    game.save = [&saves](Pokemon::StoryBackend<ZonePose>&){
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
    {.input = {"01.00 opening: A at a dialog, a fade waited out, the quiet end", test_opening_scene}, .expectation = {}},
    {.input = {"01.00 opening: no quiet end before the scene's length", test_opening_waits_min_length}, .expectation = {}},
    {.input = {"01.00 text_to_choice: B through the text, done at the choice", test_text_to_choice}, .expectation = {}},
    {.input = {"01.00 platform_to_plaza: no walk without free control", test_walk_needs_free_control}, .expectation = {}},
    {.input = {"01.00 platform_to_plaza: ends on the white fade", test_walk_ends_on_white_fade}, .expectation = {}},
    {.input = {"01.00 platform_to_plaza: fails at its bound", test_walk_fails_at_bound}, .expectation = {}},
    {.input = {"refused: tile walks, console actions off a console, unknown actions", test_refused_steps}, .expectation = {}},
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
