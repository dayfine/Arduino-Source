/*  PokemonLZA AutoStory - Engine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00's scene steps, the walks' compass turns, and every checkpoint, through the shared
 *  story engine on its scripted backend (FakeStoryBackend).
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <iterator>
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
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
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
    const Pokemon::StoryGame<ZonePose> game = make_offline_plza_story_game();
    Pokemon::StoryEngine<ZonePose> engine(game, backend);
    Pokemon::RunState state;
    return engine.run_checkpoint(checkpoint, Pokemon::SavePoint::RUN_END, state);
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
        return std::format("Expected inputs [{}], got [{}].", joined(expected), joined(backend.inputs()));
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
        return std::format("Expected the opening to end at the platform: {}", to_string(report.run.failure));
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

//  The opening ends on "Your train has arrived in Lumiose City!", a text box
//  with the advance arrow that waits for A (console B, 2026-11-13: the run
//  waited 100 s on it and failed platform_not_seen; one A by hand went on to
//  the platform). One A per text, none while it is still up, then the platform.
UnitTestResult test_opening_presses_through_arrival_text(CancellableScope& scope){
    const auto arrival = [](Milliseconds at){
        return with_reading(look_at(at, {EngineEvent::TEXT_BOX}), ADVANCE_ARROW_READING);
    };
    FakeBackend backend = fake_plza({
        look_at(0ms, {EngineEvent::FADE_BLACK}),
        with_reading(look_at(300ms), AUTOSAVE_READING),
        arrival(600ms),
        arrival(900ms),
        with_reading(look_at(1200ms), PLATFORM_READING),
        with_reading(look_at(1500ms), PLATFORM_READING),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.00", "opening"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return std::format("Expected the opening to end at the platform: {}", to_string(report.run.failure));
    }
    std::vector<std::string> presses;
    std::copy_if(backend.inputs().begin(), backend.inputs().end(), std::back_inserter(presses), [](const std::string& input){
        return input.starts_with("press");
    });
    if (presses != std::vector<std::string>{"press A"}){
        return std::format("Expected one A at the arrival text, got [{}].", joined(backend.inputs()));
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
        return std::format("Expected to face 90 degrees: {}", to_string(faced));
    }
    const UnitTestResult turned = expect_inputs(backend, {"press L", "stick 90deg 150ms walk, release 100ms"});
    if (turned.result != UnitTestResult::PASSED){
        return turned;
    }
    FakeBackend blind = fake_plza({look_at(0ms)}, scope);
    const Pokemon::Outcome unread = face_heading(blind, PlzaReader(), 90, 10);
    if (unread.failure != "heading_unread"){
        return std::format("Expected heading_unread with no heading read, got {}", to_string(unread));
    }
    return expect_inputs(blind, {});
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
        return std::format("Expected the new game's language to need a console, got {}", to_string(report.run.failure));
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
        game.actions.push_back({.spec = spec, .handler = [&trace, label](Pokemon::StoryBackend<ZonePose>&, const Pokemon::StoryActionCall&, Pokemon::RunState&){
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
        const Pokemon::StoryEngineReport report = engine.run_checkpoint(checkpoint, Pokemon::SavePoint::RUN_END, state);
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
        : UnitTest(std::format("PokemonLZA::AutoStoryEngine - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        try{
            return m_case.input.body(scope);
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    EngineTestCase m_case;
};

} // namespace


const EngineTestCase ENGINE_TEST_CASES[] = {
    {.input = {"01.00 opening: no presses, done on two looks at the platform after the autosave", test_opening_waits_for_platform}, .expectation = {}},
    {.input = {"01.00 opening: one A at the arrival text with the advance arrow", test_opening_presses_through_arrival_text}, .expectation = {}},
    {.input = {"face_heading: turns by the minimap's heading", test_face_heading}, .expectation = {}},
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
