/*  Story Engine Save and Retry Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checkpoint saves, segment retries, verification runs and failures that
 *  must stop instead of retrying.
 *
 */

#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryEngine.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryEngineSaveRetry_Tests.h"

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

StoryStep save_step(std::string name){
    StoryStep step;
    step.kind = StoryStepKind::SAVE;
    step.name = std::move(name);
    return step;
}

//  Gap 6: the save is told its checkpoint, at the end and at a SAVE step.
std::string save_gets_checkpoint(CancellableScope& scope){
    StoryGame<GridPose> game = flaky_game<GridPose>(0);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    const StoryCheckpoint checkpoint = in_phase(
        checkpoint_of("02.01", {game_step("walk", "fine"), save_step("save_here")}), StoryPhase::REPRODUCTION, engine
    );
    RunState state;
    const StoryEngineReport report = engine.run_checkpoint(checkpoint, SavePoint::CHECKPOINT_END, state);
    if (!report.saved){
        return "expected the checkpoint to save at its end";
    }
    const size_t saves = count_lines(backend, "[Test] save 02.01");
    return saves == 2 ? "" : std::format("expected 2 saves of 02.01, got {}: {}", saves, joined(backend.lines()));
}

//  A SAVE step part-way through a checkpoint moves the save: a reload
//  after a later failure would not return to the checkpoint's start, so
//  the segment stops there instead of retrying from step 1.
std::string no_retry_after_a_save_step(CancellableScope& scope){
    StoryGame<GridPose> game = flaky_game<GridPose>(1);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    const std::vector<StoryCheckpoint> checkpoints{in_phase(
        checkpoint_of("02.01", {game_step("walk", "fine"), save_step("save_here"), game_step("talk", "flaky")}),
        StoryPhase::REPRODUCTION, engine
    )};
    RunState state;
    const SegmentReport report = engine.run_segment("seg", checkpoints, 3, state);
    if (report.completed || report.failed_event != "talk" || report.failure.failure != "flaky_failed"){
        return std::format("expected the segment to stop at talk, got {}: {}", to_string(report.failure), joined(backend.lines()));
    }
    if (count_lines(backend, "[Test] reload") != 0){
        return std::format("expected no reload: {}", joined(backend.lines()));
    }
    return has_line(backend, "[Checkpoint] 02.01: a save step saved part-way, so a reload would not return to its start: no retry")
        ? ""
        : std::format("expected the no-retry line: {}", joined(backend.lines()));
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
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
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
struct SegmentCase{
    struct Input{
        StoryPhase phase;
        //  Which checkpoint fails once: 0 or 1.
        size_t flaky = 0;
    };

    struct Expectation{
        bool completed = false;
        size_t reloads = 0;
        size_t saves = 0;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const SegmentCase SEGMENT_CASES[] = {
    {.name = "first retries while testing", .input = {.phase = StoryPhase::TESTING, .flaky = 0},
     .expectation = {.completed = true, .reloads = 1, .saves = 0}},
    {.name = "second does not retry while testing", .input = {.phase = StoryPhase::TESTING, .flaky = 1},
     .expectation = {.completed = false, .reloads = 0, .saves = 0}},
    {.name = "second does not retry while verifying", .input = {.phase = StoryPhase::VERIFICATION, .flaky = 1},
     .expectation = {.completed = false, .reloads = 0, .saves = 0}},
    {.name = "second retries after a reproduction save", .input = {.phase = StoryPhase::REPRODUCTION, .flaky = 1},
     .expectation = {.completed = true, .reloads = 1, .saves = 2}},
};

std::string check_segment(const SegmentCase& test, CancellableScope& scope){
    const StoryGame<GridPose> game = flaky_game<GridPose>(1);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
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
        : UnitTest(std::format("Pokemon::StoryEngine segment - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = check_segment(m_case, scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    SegmentCase m_case;
};

//  The engine's retries run through run_checkpoint_attempts(): the same
//  reload and pass lines and attempt counts as its own loop had.
struct RetryCase{
    struct Input{
        //  Calls of the checkpoint's action that fail before one passes.
        size_t failures = 0;
        size_t attempts = 0;
    };

    struct Expectation{
        bool passed = false;
        size_t attempts = 0;
        //  Lines the run writes once each.
        std::vector<std::string_view> lines;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const RetryCase RETRY_CASES[] = {
    {.name = "pass after a reload", .input = {.failures = 1, .attempts = 3},
     .expectation = {.passed = true, .attempts = 2, .lines = {
        "[Checkpoint] 01: failed, resetting for attempt 2/3",
        "[Test] reload",
        "[Checkpoint] 01: passed on attempt 2 (1 reset)",
     }}},
    {.name = "stop after the last attempt", .input = {.failures = 5, .attempts = 2},
     .expectation = {.passed = false, .attempts = 2, .lines = {
        "[Checkpoint] 01: failed, resetting for attempt 2/2",
        "[Test] reload",
     }}},
};

std::string check_retry(const RetryCase& test, CancellableScope& scope){
    const StoryGame<GridPose> game = flaky_game<GridPose>(test.input.failures);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    const std::vector<StoryCheckpoint> checkpoints{checkpoint_of("01", {game_step("walk", "flaky")})};
    RunState state;
    const SegmentReport report = engine.run_segment("seg", checkpoints, test.input.attempts, state);
    const CheckpointRun& run = report.runs.at(0);
    if (run.passed != test.expectation.passed || run.attempts != test.expectation.attempts){
        return std::format(
            "expected passed {} after {} attempts, got {} after {}: {}",
            test.expectation.passed, test.expectation.attempts, run.passed, run.attempts, joined(backend.lines())
        );
    }
    for (std::string_view line : test.expectation.lines){
        if (count_lines(backend, line) != 1){
            return std::format("expected \"{}\" once: {}", line, joined(backend.lines()));
        }
    }
    return "";
}

class StoryRetryTest : public UnitTest{
public:
    explicit StoryRetryTest(RetryCase test)
        : UnitTest(std::format("Pokemon::StoryEngine retry - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = check_retry(m_case, scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    RetryCase m_case;
};

//  Gap 7: verify runs a checkpoint 3 times from one save, saving only at
//  the end of the last, and only when every run passed.
std::string verify_saves_after_all_passed(CancellableScope& scope){
    for (size_t failures = 0; failures < 2; failures++){
        const StoryGame<GridPose> game = flaky_game<GridPose>(failures);
        FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
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

struct RunsCase{
    //  One check above, by name.
    struct Input{
        std::string (*check)(CancellableScope& scope);
    };

    //  The check's error: none.
    struct Expectation{
        std::string_view error = {};
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const RunsCase RUNS_CASES[] = {
    {.name = "save gets its checkpoint", .input = {.check = &save_gets_checkpoint}, .expectation = {}},
    {.name = "no retry after a save step", .input = {.check = &no_retry_after_a_save_step}, .expectation = {}},
    {.name = "console_only refused off console", .input = {.check = &console_only_refused}, .expectation = {}},
    {.name = "no reload for console_only", .input = {.check = &segment_no_reload_for_console_only}, .expectation = {}},
    {.name = "verify saves after all passed", .input = {.check = &verify_saves_after_all_passed}, .expectation = {}},
};

class StoryEngineRunsTest : public UnitTest{
public:
    explicit StoryEngineRunsTest(RunsCase test)
        : UnitTest(std::format("Pokemon::StoryEngine runs - {}", test.name))
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


void add_tests_StoryEngineSaveRetry(UnitTestDatabase& database){
    for (const RunsCase& test : RUNS_CASES){
        database.add<StoryEngineRunsTest>(test);
    }
    for (const SegmentCase& test : SEGMENT_CASES){
        database.add<StorySegmentTest>(test);
    }
    for (const RetryCase& test : RETRY_CASES){
        database.add<StoryRetryTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
