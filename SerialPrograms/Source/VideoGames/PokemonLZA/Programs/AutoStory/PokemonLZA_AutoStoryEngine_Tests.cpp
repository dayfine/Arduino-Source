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
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game(StarterChoice::CHIKORITA);
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

//  01.01's start check: one look at the platform is not enough, and with
//  no platform it fails within its 10 s, pressing nothing.
UnitTestResult test_start_check_needs_platform(CancellableScope& scope){
    FakeBackend backend = fake_plza({
        with_reading(look_at(0ms), PLATFORM_READING),
        look_at(500ms),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "on_platform"));
    if (report.run.completed || report.run.failure.failure != "platform_not_seen"){
        return std::format("Expected platform_not_seen, got {}", to_string(report.run.failure));
    }
    for (const std::string& input : backend.inputs()){
        if (input != "wait 300ms"){
            return std::format("Expected only waits, got {}", input);
        }
    }
    return true;
}

//  A walk starts only on free control: platform_to_plaza on a text box
//  waits for it, then fails with no input but its waits.
UnitTestResult test_walk_needs_free_control(CancellableScope& scope){
    FakeBackend backend = fake_plza({look_at(0ms, {EngineEvent::TEXT_BOX})}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "platform_to_plaza"));
    if (report.run.completed || report.run.failure.failure != "walk_not_free"){
        return std::format("Expected walk_not_free, got {}", to_string(report.run.failure));
    }
    for (const std::string& input : backend.inputs()){
        if (input != "wait 250ms"){
            return std::format("Expected only waits, got {}", input);
        }
    }
    return true;
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
        return std::format("Expected the walk to end on the white fade: {}", to_string(report.run.failure));
    }
    return expect_inputs(backend, {
        "wait 250ms", "hold north 4500ms walk", "press L", "stick 90deg 150ms walk, release 100ms", "press L",
        "hold north 250ms walk", "hold north 250ms walk", "hold north 250ms walk",
    });
}

//  platform_to_plaza with no white fade fails once the last leg's 10 s are
//  held: 40 slices.
UnitTestResult test_walk_fails_at_bound(CancellableScope& scope){
    FakeBackend backend = fake_plza({look_at(0ms)}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "platform_to_plaza"));
    if (report.run.completed || report.run.failure.failure != "walk_no_end"){
        return std::format("Expected walk_no_end, got {}", to_string(report.run.failure));
    }
    const size_t slices = (size_t)std::count(backend.inputs().begin(), backend.inputs().end(), "hold north 250ms walk");
    if (slices != 40){
        return std::format("Expected 40 slices of 250 ms, held {}.", slices);
    }
    return true;
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

//  Pancham cornered (text_to_choice): B through the text, done at the
//  choice without pressing it.
UnitTestResult test_text_to_choice(CancellableScope& scope){
    const Observation<ZonePose> choice = look_at(2000ms, {EngineEvent::TEXT_BOX, EngineEvent::CHOICE_BOX});
    FakeBackend backend = fake_plza({look_at(0ms, {EngineEvent::TEXT_BOX}), choice}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.02", "pancham_cornered"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return std::format("Expected the scene to end at the choice: {}", to_string(report.run.failure));
    }
    return expect_inputs(backend, {"press B"});
}

//  01.01's arrival scenes (mash_to_move_hints): A at the text and at the
//  choice (its top row), one blind A in a 20 s text-free flyover, done
//  once the controls hint has shown a second.
UnitTestResult test_arrival_ends_on_controls_hint(CancellableScope& scope){
    FakeBackend backend = fake_plza({
        look_at(0ms, {EngineEvent::TEXT_BOX}),
        look_at(1000ms),
        look_at(21000ms),
        look_at(22000ms, {EngineEvent::TEXT_BOX, EngineEvent::CHOICE_BOX}),
        look_at(23000ms),
        with_reading(look_at(24000ms), CONTROLS_HINT_READING),
        with_reading(look_at(24500ms), CONTROLS_HINT_READING),
        with_reading(look_at(25100ms), CONTROLS_HINT_READING),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "arrival_scenes"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return std::format("Expected the scenes to end on the controls hint: {}", to_string(report.run.failure));
    }
    const auto presses = std::count_if(backend.inputs().begin(), backend.inputs().end(), [](const std::string& input){
        return input.starts_with("press");
    });
    const auto a_presses = std::count(backend.inputs().begin(), backend.inputs().end(), "press A");
    if (presses != 3 || a_presses != 3){
        return std::format("Expected A at the text, once in the flyover and at the choice, got [{}].", joined(backend.inputs()));
    }
    return true;
}

//  01.01's ad (run 2, 2026-11-10 13:28): subtitles with no box. A at each
//  advance chevron, a blind A once nothing has shown for 5 s, none once
//  the controls hint shows; done when it has shown a second.
//  01.02's battle area (console B, 2026-11-10 14:53): A at the text, a
//  fade, then free control with only the "A: Talk" prompt, no controls
//  hint. The prompt held a second ends the step; no press at it.
UnitTestResult test_battle_area_ends_on_interact_prompt(CancellableScope& scope){
    FakeBackend backend = fake_plza({
        look_at(0ms, {EngineEvent::TEXT_BOX}),
        look_at(1000ms, {EngineEvent::FADE_BLACK}),
        look_at(3000ms),
        look_at(3250ms, {EngineEvent::INTERACT_PROMPT}),
        look_at(3750ms, {EngineEvent::INTERACT_PROMPT}),
        look_at(4300ms, {EngineEvent::INTERACT_PROMPT}),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.02", "battle_area"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return std::format("Expected the battle area to end on the interact prompt: {}", to_string(report.run.failure));
    }
    std::vector<std::string> presses;
    std::copy_if(backend.inputs().begin(), backend.inputs().end(), std::back_inserter(presses), [](const std::string& input){
        return input.starts_with("press");
    });
    if (presses != std::vector<std::string>{"press A"}){
        return std::format("Expected one A at the text, got [{}].", joined(backend.inputs()));
    }
    return true;
}

UnitTestResult test_arrival_presses_through_the_ad(CancellableScope& scope){
    FakeBackend backend = fake_plza({
        look_at(0ms),
        with_reading(look_at(1000ms), ADVANCE_ARROW_READING),
        with_reading(look_at(2000ms), ADVANCE_ARROW_READING),
        look_at(2500ms),
        look_at(6000ms),
        look_at(7100ms),
        with_reading(look_at(8000ms), CONTROLS_HINT_READING),
        with_reading(look_at(8500ms), CONTROLS_HINT_READING),
        with_reading(look_at(9100ms), CONTROLS_HINT_READING),
    }, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "arrival_scenes"));
    if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
        return std::format("Expected the ad to end on the controls hint: {}", to_string(report.run.failure));
    }
    std::vector<std::string> presses;
    std::copy_if(backend.inputs().begin(), backend.inputs().end(), std::back_inserter(presses), [](const std::string& input){
        return input.starts_with("press");
    });
    if (presses != std::vector<std::string>{"press A", "press A", "press A"}){
        return std::format("Expected A at the two chevrons and one blind A, got [{}].", joined(backend.inputs()));
    }
    return true;
}

//  01.01's arrival (run 3, 2026-11-10 13:49): A at the last box, then the
//  "MAIN MISSION 01" banner on two looks ends the step, which returns
//  once the banner has gone, with no press at it. A banner still up after
//  10 s (01.03's "MAIN MISSION 02", console B 2026-11-10) gets A 3 times,
//  then the step returns all the same.
UnitTestResult test_arrival_ends_on_mission_banner(CancellableScope& scope){
    for (const bool goes : {true, false}){
        std::vector<Observation<ZonePose>> looks{
            look_at(0ms, {EngineEvent::TEXT_BOX}),
            look_at(500ms),
            with_reading(look_at(1000ms), MISSION_BANNER_READING),
            with_reading(look_at(1250ms), MISSION_BANNER_READING),
            with_reading(look_at(1500ms), MISSION_BANNER_READING),
        };
        if (goes){
            looks.push_back(look_at(4000ms));
        }
        FakeBackend backend = fake_plza(std::move(looks), scope);
        const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "arrival_scenes"));
        if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
            return std::format("Expected the scenes to end on the mission banner: {}", to_string(report.run.failure));
        }
        const auto presses = std::count_if(backend.inputs().begin(), backend.inputs().end(), [](const std::string& input){
            return input.starts_with("press");
        });
        const auto expected = goes ? 1 : 4;
        if (presses != expected){
            return std::format("Expected {} A presses, got [{}].", expected, joined(backend.inputs()));
        }
    }
    return true;
}

//  01.03's after_andi (console B, 2026-11-10): A at the box, A at each
//  look at the "MAIN MISSION 01 COMPLETE" card, then "MAIN MISSION 02" on
//  two looks ends the step. A banner that goes by itself gets no press; one
//  that stays past 10 s gets A 3 times.
UnitTestResult test_after_andi_presses_complete_card(CancellableScope& scope){
    for (const bool goes : {true, false}){
        std::vector<Observation<ZonePose>> looks{
            look_at(0ms, {EngineEvent::TEXT_BOX}),
            with_reading(look_at(500ms), MISSION_COMPLETE_READING),
            with_reading(look_at(1000ms), MISSION_COMPLETE_READING),
            look_at(1500ms),
            with_reading(look_at(2000ms), MISSION_BANNER_READING),
            with_reading(look_at(2250ms), MISSION_BANNER_READING),
        };
        if (goes){
            looks.push_back(look_at(2500ms));
        }
        FakeBackend backend = fake_plza(std::move(looks), scope);
        const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.03", "after_andi"));
        if (report.verdict != Pokemon::CheckpointVerdict::PASSED){
            return std::format("Expected after_andi to end on the mission banner: {}", to_string(report.run.failure));
        }
        const auto presses = std::count_if(backend.inputs().begin(), backend.inputs().end(), [](const std::string& input){
            return input.starts_with("press");
        });
        const auto expected = goes ? 3 : 6;
        if (presses != expected){
            return std::format("Expected {} A presses (the box, the card twice, the banner if it stays), got [{}].",
                expected, joined(backend.inputs()));
        }
    }
    return true;
}

//  01.01's arrival scenes: a minute with nothing busy and no controls hint
//  (a text-free cutscene, or control the detectors miss) fails the step
//  instead of taking it for control.
UnitTestResult test_arrival_quiet_fails(CancellableScope& scope){
    FakeBackend backend = fake_plza({look_at(0ms), look_at(30000ms), look_at(61000ms)}, scope);
    const Pokemon::StoryEngineReport report = run_plza(backend, step_of("01.01", "arrival_scenes"));
    if (report.verdict == Pokemon::CheckpointVerdict::PASSED || report.run.failure.failure != "scene_quiet"){
        return std::format("Expected a quiet minute to fail as scene_quiet, got {}", to_string(report.run.failure));
    }
    return true;
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
    {.input = {"01.01 start check: fails without the platform", test_start_check_needs_platform}, .expectation = {}},
    {.input = {"01.01 text_to_choice: B through the text, done at the choice", test_text_to_choice}, .expectation = {}},
    {.input = {"01.01 arrival_scenes: A through text and choice, done on the controls hint", test_arrival_ends_on_controls_hint}, .expectation = {}},
    {.input = {"01.01 arrival_scenes: A through the ad's boxless subtitles", test_arrival_presses_through_the_ad}, .expectation = {}},
    {.input = {"01.01 arrival_scenes: done on the mission banner once it has gone, A if it stays", test_arrival_ends_on_mission_banner}, .expectation = {}},
    {.input = {"01.03 after_andi: A at the mission complete card, then the banner, A if it stays", test_after_andi_presses_complete_card}, .expectation = {}},
    {.input = {"01.01 arrival_scenes: a quiet minute without the hint fails", test_arrival_quiet_fails}, .expectation = {}},
    {.input = {"01.02 battle_area: done on the \"A: Talk\" prompt", test_battle_area_ends_on_interact_prompt}, .expectation = {}},
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
