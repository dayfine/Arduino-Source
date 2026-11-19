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
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
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

StoryAction game_action(std::string name, StoryActionHandler handler){
    return StoryAction{
        .spec = {.kind = StoryStepKind::GAME, .name = std::move(name), .args = {}},
        .handler = std::move(handler),
    };
}

//  The test game. Its save fails at the checkpoints of `save_failures`; its
//  reload presses HOME and answers `reload_start`.
StoryGame test_game(std::set<std::string> save_failures, std::optional<std::string> reload_start, TestWorld& world){
    StoryGame game;
    game.id = "test";
    game.actions.push_back(game_action("ok", [](StoryBackend&, const StoryActionCall&, RunState&){
        return std::optional<StoryFailure>();
    }));
    game.actions.push_back(game_action("fail", [](StoryBackend&, const StoryActionCall&, RunState&){
        return std::optional<StoryFailure>(StoryFailure{.code = "scripted_failure"});
    }));
    game.actions.push_back(game_action("fail_once", [&world](StoryBackend&, const StoryActionCall&, RunState&){
        return world.fail_once_calls++ == 0 ? std::optional<StoryFailure>(StoryFailure{.code = "scripted_failure"}) : std::nullopt;
    }));
    game.actions.push_back(game_action("not_running", [](StoryBackend&, const StoryActionCall&, RunState&){
        return std::optional<StoryFailure>(game_not_running_failure());
    }));
    game.actions.push_back(game_action("press_a", [](StoryBackend& backend, const StoryActionCall&, RunState&){
        backend.press(NintendoSwitch::BUTTON_A, /*hold=*/80ms, /*release=*/80ms);
        return std::optional<StoryFailure>();
    }));
    //  Walks north until a look shows a white fade, at most 3 slices.
    game.actions.push_back(game_action("walk_to_fade", [](StoryBackend& backend, const StoryActionCall&, RunState&){
        for (size_t slice = 0; slice < 3; slice++){
            backend.stick(/*degrees=*/0, /*duration=*/500ms, /*release=*/0ms, StoryPace::WALK);
            if (backend.observe().events.has(ScreenEvent::FADE_WHITE)){
                return std::optional<StoryFailure>();
            }
        }
        return std::optional<StoryFailure>(StoryFailure{.code = "no_fade", .detail = "no white fade in 3 slices"});
    }));
    game.save = [save_failures = std::move(save_failures)](StoryBackend&, const StoryCheckpoint& checkpoint){
        if (save_failures.contains(checkpoint.id)){
            return std::optional<StoryFailure>(StoryFailure{.code = "save_error", .detail = "the disk is full"});
        }
        return std::optional<StoryFailure>();
    };
    game.reload = [reload_start = std::move(reload_start)](StoryBackend& backend, const StoryCheckpoint&){
        backend.press(NintendoSwitch::BUTTON_HOME, /*hold=*/80ms, /*release=*/80ms);
        return reload_start;
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
ScreenObservation look(ScreenEvents events){
    ScreenObservation observation;
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

//  "done" or "failed at 01.01 fail: scripted_failure" (no step name when the
//  checkpoint failed outside its steps).
std::string verdict(const std::optional<CheckpointFailure>& failure, const std::vector<StoryCheckpoint>& checkpoints){
    if (!failure){
        return "done";
    }
    std::string event;
    for (const StoryCheckpoint& checkpoint : checkpoints){
        if (checkpoint.id == failure->checkpoint_id && failure->event_index){
            event = checkpoint.steps[*failure->event_index].name;
        }
    }
    return std::format("failed at {} {}: {}", failure->checkpoint_id, event, failure->failure.code);
}


struct RangeCase{
    std::string name = {};
    struct Input{
        std::vector<StoryCheckpoint> checkpoints = {};
        size_t attempts = 1;
        std::set<std::string> save_failures = {};
        //  What the reload answers.
        std::optional<std::string> reload_start = std::nullopt;
        std::vector<ScreenObservation> looks = {};
    };
    Input input = {};
    struct Expectation{
        //  verdict() of the run.
        std::string verdict = {};
        //  The backend's inputs, joined by ", ".
        std::string inputs = {};
        //  A log line the run wrote, if any is checked.
        std::string line = {};
    };
    Expectation expectation = {};
};

const RangeCase RANGE_CASES[] = {
    {
        .name = "two checkpoints pass",
        .input = {.checkpoints = {checkpoint_of("01.00", {"ok", "press_a"}), checkpoint_of("01.01", {"ok"})}},
        .expectation = {.verdict = "done", .inputs = "press A", .line = "[Checkpoint] 01.01: passed on attempt 1 (one-shot)"},
    },
    {
        .name = "a save that fails is logged and the run goes on",
        .input = {.checkpoints = {checkpoint_of("01.00", {"ok"}), checkpoint_of("01.01", {"press_a"})}, .save_failures = {"01.00"}},
        .expectation = {.verdict = "done", .inputs = "press A", .line = "[Save] 01.00: save failed: save_error: the disk is full"},
    },
    {
        .name = "a failed step stops the run at its event",
        .input = {.checkpoints = {checkpoint_of("01.00", {"ok", "fail", "press_a"})}},
        .expectation = {.verdict = "failed at 01.00 fail: scripted_failure", .inputs = "", .line = "[Checkpoint] 01.00: failed at 2 fail: scripted_failure"},
    },
    {
        .name = "a failed checkpoint passes after a reload to its start",
        .input = {.checkpoints = {checkpoint_of("01.01", {"fail_once"})}, .attempts = 3, .reload_start = "01.01"},
        .expectation = {.verdict = "done", .inputs = "press HOME", .line = "[Checkpoint] 01.01: passed on attempt 2 (1 reset)"},
    },
    {
        .name = "a reload to an earlier checkpoint's start replays it",
        .input = {
            .checkpoints = {checkpoint_of("01.00", {"press_a"}), checkpoint_of("01.01", {"fail_once"})},
            .attempts = 2,
            .reload_start = "01.00",
        },
        .expectation = {.verdict = "done", .inputs = "press A, press HOME, press A", .line = "[Checkpoint] 01.01: passed on attempt 2 (1 reset)"},
    },
    {
        .name = "a checkpoint stops the run after its last attempt fails",
        .input = {.checkpoints = {checkpoint_of("01.01", {"fail"})}, .attempts = 2, .reload_start = "01.01"},
        .expectation = {.verdict = "failed at 01.01 fail: scripted_failure", .inputs = "press HOME"},
    },
    {
        .name = "a reload to an unknown start stops the run",
        .input = {.checkpoints = {checkpoint_of("01.01", {"fail"})}, .attempts = 3},
        .expectation = {.verdict = "failed at 01.01 : start_unknown", .inputs = "press HOME", .line = "[Checkpoint] 01.01: reloaded to no start of this run"},
    },
    {
        .name = "a failure that repeats stops the run with no reload",
        .input = {.checkpoints = {checkpoint_of("01.01", {"not_running"})}, .attempts = 3, .reload_start = "01.01"},
        .expectation = {.verdict = "failed at 01.01 not_running: game_not_running", .inputs = ""},
    },
    {
        .name = "an unknown action fails the lint before any input",
        .input = {.checkpoints = {checkpoint_of("01.00", {"press_a", "dance"})}, .attempts = 3, .reload_start = "01.00"},
        .expectation = {.verdict = "failed at 01.00 : lint_failed", .inputs = "", .line = "[Lint] 01.00 step 2 dance: unknown action \"dance\" for GAME"},
    },
    {
        .name = "a walk ends on the scripted white fade",
        .input = {.checkpoints = {checkpoint_of("01.01", {"walk_to_fade"})}, .looks = {look({}), look({ScreenEvent::FADE_WHITE})}},
        .expectation = {.verdict = "done", .inputs = "stick 0deg 500ms walk, stick 0deg 500ms walk"},
    },
    {
        .name = "a walk whose fade never shows fails",
        .input = {.checkpoints = {checkpoint_of("01.01", {"walk_to_fade"})}, .looks = {look({ScreenEvent::DIALOG_BOX})}},
        .expectation = {
            .verdict = "failed at 01.01 walk_to_fade: no_fade",
            .inputs = "stick 0deg 500ms walk, stick 0deg 500ms walk, stick 0deg 500ms walk",
            .line = "[Step] walk_to_fade GAME/walk_to_fade: failed: no_fade: no white fade in 3 slices",
        },
    },
};

UnitTestResult check_range(const RangeCase& test, CancellableScope& scope){
    TestWorld world;
    StoryGame game = test_game(test.input.save_failures, test.input.reload_start, world);
    game.attempts_per_checkpoint = test.input.attempts;
    FakeStoryBackend backend({.looks = test.input.looks}, scope);
    RunState state;
    const std::optional<CheckpointFailure> failure = run_checkpoints(backend, std::span<const StoryCheckpoint>(test.input.checkpoints), game, state);
    const std::string got = verdict(failure, test.input.checkpoints);
    if (got != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.verdict, got);
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
    explicit StoryEngineTest(RangeCase test_case)
        : UnitTest(std::format("Pokemon::StoryEngine - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_range(m_case, scope);
    }

private:
    RangeCase m_case;
};

} // namespace


void add_tests_StoryEngine(UnitTestDatabase& database){
    for (const RangeCase& test : RANGE_CASES){
        database.add<StoryEngineTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
