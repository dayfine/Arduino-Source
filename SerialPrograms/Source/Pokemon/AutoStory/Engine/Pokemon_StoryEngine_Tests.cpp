/*  Story Engine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The engine on a FakeStoryBackend: a test game whose actions answer as
 *  scripted (pass, fail, fail once, press, wait for a white fade in the
 *  scripted looks), so a run's report, inputs and retries are checked with
 *  no console.
 *
 */

#include <format>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryEngine.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryEngine_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

using namespace std::chrono_literals;


//  What the test game's actions share across a run.
struct TestWorld{
    //  How many times "fail_once" ran.
    size_t fail_once_calls = 0;
};

StoryAction<ZonePose> game_action(std::string name, StoryActionHandler<ZonePose> handler){
    return StoryAction<ZonePose>{
        .spec = {.kind = StoryStepKind::GAME, .name = std::move(name), .args = {}, .console_only = false},
        .handler = std::move(handler),
    };
}

//  The test game. Its save saves the checkpoints of `autosaves`, fails on
//  those of `save_failures` and does not save at the others; its reload
//  works unless `reload_fails`.
StoryGame<ZonePose> test_game(
    TestWorld& world, std::set<std::string> autosaves, std::set<std::string> save_failures, bool reload_fails
){
    StoryGame<ZonePose> game;
    game.id = "test";
    game.actions.push_back(game_action("ok", [](StoryBackend<ZonePose>&, const StoryActionCall&, RunState&){
        return outcome_ok();
    }));
    game.actions.push_back(game_action("fail", [](StoryBackend<ZonePose>&, const StoryActionCall&, RunState&){
        return outcome_failed("scripted_failure");
    }));
    game.actions.push_back(game_action("fail_once", [&world](StoryBackend<ZonePose>&, const StoryActionCall&, RunState&){
        return world.fail_once_calls++ == 0 ? outcome_failed("scripted_failure") : outcome_ok();
    }));
    game.actions.push_back(game_action("not_running", [](StoryBackend<ZonePose>&, const StoryActionCall&, RunState&){
        return outcome_game_not_running();
    }));
    game.actions.push_back(game_action("press_a", [](StoryBackend<ZonePose>& backend, const StoryActionCall&, RunState&){
        backend.press(NintendoSwitch::BUTTON_A, 80ms, 80ms);
        return outcome_ok();
    }));
    StoryAction<ZonePose> console = game_action("console", [](StoryBackend<ZonePose>&, const StoryActionCall&, RunState&){
        return outcome_ok();
    });
    console.spec.console_only = true;
    game.actions.push_back(std::move(console));
    //  Walks north until a look shows a white fade, at most 3 slices.
    game.actions.push_back(game_action("walk_to_fade", [](StoryBackend<ZonePose>& backend, const StoryActionCall&, RunState&){
        for (size_t slice = 0; slice < 3; slice++){
            backend.hold(Direction::NORTH, 500ms, StoryPace::WALK);
            if (backend.observe().events.has(ScreenEvent::FADE_WHITE)){
                return outcome_ok();
            }
        }
        return outcome_failed("no_fade", "no white fade in 3 slices");
    }));
    game.save = [autosaves = std::move(autosaves), save_failures = std::move(save_failures)](
        StoryBackend<ZonePose>&, const StoryCheckpoint& checkpoint
    ){
        if (autosaves.contains(checkpoint.id)){
            return outcome_ok();
        }
        return save_failures.contains(checkpoint.id) ? outcome_failed("save_error", "the disk is full") : outcome_not_saved("no autosave here");
    };
    game.reload = [reload_fails](StoryBackend<ZonePose>& backend){
        backend.press(NintendoSwitch::BUTTON_HOME, 80ms, 80ms);
        return reload_fails ? outcome_failed("no_game") : outcome_ok();
    };
    return game;
}

//  A checkpoint of GAME steps, each named for its action.
StoryCheckpoint checkpoint_of(std::string id, std::vector<std::string> actions){
    StoryCheckpoint checkpoint;
    checkpoint.id = std::move(id);
    checkpoint.title = "Test";
    for (std::string& action : actions){
        checkpoint.steps.push_back(StoryStep{.kind = StoryStepKind::GAME, .name = action, .why = "", .action = action, .args = {}});
    }
    return checkpoint;
}

//  A look that shows `events`.
Observation<ZonePose> look(ScreenEvents events){
    Observation<ZonePose> observation;
    observation.events = events;
    return observation;
}

//  "a b": the strings, space-separated.
std::string joined(const std::vector<std::string>& texts, std::string_view separator = " "){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : std::string(separator)) + part;
    }
    return text;
}

//  "done | runs 01.00:pass/1 | reloads 0" or
//  "failed at 01.01 fail: scripted_failure | runs ... | reloads 1".
std::string summary(const SegmentReport& report){
    std::vector<std::string> runs;
    for (const CheckpointRun& run : report.runs){
        runs.push_back(std::format("{}:{}/{}", run.id, run.passed ? "pass" : "fail", run.attempts));
    }
    const std::string verdict = report.completed
        ? "done"
        : std::format("failed at {} {}: {}", report.failed_checkpoint, report.failed_event, report.failure.failure);
    return std::format("{} | runs {} | reloads {}", verdict, joined(runs), report.reloads);
}


struct SegmentCase{
    std::string name;
    struct Input{
        std::vector<StoryCheckpoint> checkpoints;
        size_t attempts = 1;
        std::set<std::string> autosaves = {};
        std::set<std::string> save_failures = {};
        bool reload_fails = false;
        std::vector<Observation<ZonePose>> looks = {};
        StoryBackendKind kind = StoryBackendKind::SCRIPT;
    };
    Input input;
    struct Expectation{
        //  summary() of the segment's report.
        std::string summary;
        //  The backend's inputs, joined by ", ".
        std::string inputs = {};
        //  A log line the run wrote, if any is checked.
        std::string line = {};
    };
    Expectation expectation;
};

const SegmentCase SEGMENT_CASES[] = {
    {
        .name = "two checkpoints pass and save",
        .input = {.checkpoints = {checkpoint_of("01.00", {"ok", "press_a"}), checkpoint_of("01.01", {"ok"})}, .autosaves = {"01.00", "01.01"}},
        .expectation = {.summary = "done | runs 01.00:pass/1 01.01:pass/1 | reloads 0", .inputs = "press A", .line = "[Save] 01.00: saved"},
    },
    {
        .name = "a checkpoint the game does not save at is not saved, with the reason",
        .input = {.checkpoints = {checkpoint_of("01.00", {"ok"})}},
        .expectation = {.summary = "done | runs 01.00:pass/1 | reloads 0", .inputs = "", .line = "[Save] 01.00: not saved (no autosave here)"},
    },
    {
        .name = "a save that fails is logged as failed",
        .input = {.checkpoints = {checkpoint_of("01.00", {"ok"})}, .save_failures = {"01.00"}},
        .expectation = {.summary = "done | runs 01.00:pass/1 | reloads 0", .inputs = "", .line = "[Save] 01.00: save failed: save_error: the disk is full"},
    },
    {
        .name = "a failed step stops the segment with its event",
        .input = {.checkpoints = {checkpoint_of("01.00", {"ok", "fail", "press_a"})}},
        .expectation = {.summary = "failed at 01.00 fail: scripted_failure | runs 01.00:fail/1 | reloads 0", .inputs = "", .line = "[Checkpoint] 01.00: failed at 2 fail: scripted_failure"},
    },
    {
        .name = "a failed checkpoint passes after a reload",
        .input = {.checkpoints = {checkpoint_of("01.01", {"fail_once"})}, .attempts = 3},
        .expectation = {.summary = "done | runs 01.01:pass/2 | reloads 1", .inputs = "press Home", .line = "[Checkpoint] 01.01: failed, resetting for attempt 2/3"},
    },
    {
        .name = "no reload after a checkpoint that did not save",
        .input = {.checkpoints = {checkpoint_of("01.00", {"ok"}), checkpoint_of("01.01", {"fail"})}, .attempts = 3},
        .expectation = {.summary = "failed at 01.01 fail: scripted_failure | runs 01.00:pass/1 01.01:fail/1 | reloads 0", .inputs = "", .line = "[Segment] test: 01.01: no save at its start, so no reload if it fails"},
    },
    {
        .name = "a reload after a checkpoint that saved",
        .input = {.checkpoints = {checkpoint_of("01.00", {"ok"}), checkpoint_of("01.01", {"fail"})}, .attempts = 2, .autosaves = {"01.00"}},
        .expectation = {.summary = "failed at 01.01 fail: scripted_failure | runs 01.00:pass/1 01.01:fail/2 | reloads 1", .inputs = "press Home"},
    },
    {
        .name = "a failed reload ends the segment",
        .input = {.checkpoints = {checkpoint_of("01.01", {"fail"})}, .attempts = 3, .reload_fails = true},
        .expectation = {.summary = "failed at 01.01 : reload_failed | runs 01.01:fail/1 | reloads 0", .inputs = "press Home"},
    },
    {
        .name = "the game not running is not retried",
        .input = {.checkpoints = {checkpoint_of("01.01", {"not_running"})}, .attempts = 3},
        .expectation = {.summary = "failed at 01.01 not_running: game_not_running | runs 01.01:fail/1 | reloads 0"},
    },
    {
        .name = "an unknown action fails the lint before any input",
        .input = {.checkpoints = {checkpoint_of("01.00", {"press_a", "dance"})}, .attempts = 3},
        .expectation = {.summary = "failed at 01.00 : lint_failed | runs 01.00:fail/1 | reloads 0", .inputs = "", .line = "[Lint] 01.00 step 2 dance: unknown action \"dance\" for GAME"},
    },
    {
        .name = "a console_only action fails off a console",
        .input = {.checkpoints = {checkpoint_of("01.00", {"console"})}, .attempts = 3},
        .expectation = {.summary = "failed at 01.00 console: console_only | runs 01.00:fail/1 | reloads 0"},
    },
    {
        .name = "a console_only action runs on a console",
        .input = {.checkpoints = {checkpoint_of("01.00", {"console"})}, .kind = StoryBackendKind::CONSOLE},
        .expectation = {.summary = "done | runs 01.00:pass/1 | reloads 0"},
    },
    {
        .name = "a walk ends on the scripted white fade",
        .input = {.checkpoints = {checkpoint_of("01.01", {"walk_to_fade"})}, .looks = {look({}), look({ScreenEvent::FADE_WHITE})}},
        .expectation = {.summary = "done | runs 01.01:pass/1 | reloads 0", .inputs = "hold north 500ms walk, hold north 500ms walk"},
    },
    {
        .name = "a walk whose fade never shows fails",
        .input = {.checkpoints = {checkpoint_of("01.01", {"walk_to_fade"})}, .looks = {look({ScreenEvent::DIALOG_BOX})}},
        .expectation = {
            .summary = "failed at 01.01 walk_to_fade: no_fade | runs 01.01:fail/1 | reloads 0",
            .inputs = "hold north 500ms walk, hold north 500ms walk, hold north 500ms walk",
            .line = "[Step] walk_to_fade GAME/walk_to_fade: failed: no_fade: no white fade in 3 slices",
        },
    },
};

UnitTestResult check_segment(const SegmentCase& test, CancellableScope& scope){
    TestWorld world;
    const StoryGame<ZonePose> game = test_game(world, test.input.autosaves, test.input.save_failures, test.input.reload_fails);
    FakeStoryBackend<ZonePose> backend({.looks = test.input.looks, .kind = test.input.kind}, scope);
    StoryEngine<ZonePose> engine(game, backend);
    RunState state;
    const SegmentReport report = engine.run_segment("test", test.input.checkpoints, test.input.attempts, state);
    const std::string got = summary(report);
    if (got != test.expectation.summary){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.summary, got);
    }
    const std::string inputs = joined(backend.inputs(), ", ");
    if (inputs != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, inputs);
    }
    if (test.expectation.line.empty()){
        return true;
    }
    for (const std::string& line : backend.lines()){
        if (line == test.expectation.line){
            return true;
        }
    }
    return std::format("expected the line \"{}\" in:\n{}", test.expectation.line, joined(backend.lines(), "\n"));
}


class StoryEngineTest : public UnitTest{
public:
    explicit StoryEngineTest(SegmentCase test_case)
        : UnitTest(std::format("Pokemon::StoryEngine - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_segment(m_case, scope);
    }

private:
    SegmentCase m_case;
};

} // namespace


void add_tests_StoryEngine(UnitTestDatabase& database){
    for (const SegmentCase& test : SEGMENT_CASES){
        database.add<StoryEngineTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
