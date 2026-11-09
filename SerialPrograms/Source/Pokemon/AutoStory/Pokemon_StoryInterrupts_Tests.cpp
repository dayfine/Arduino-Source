/*  Story Interrupts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The table executor's looks between steps, against a fake game: a
 *  scripted screen and handlers that note what they cleared.
 *
 */

#include <algorithm>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_StoryActions.h"
#include "Pokemon_StoryInterrupts.h"
#include "Pokemon_AutoStory_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

const WallClock T0 = WallClock() + std::chrono::hours(1);

//  One frame of a scripted screen: its events, at `ms` after T0.
struct Frame{
    int ms;
    ScreenEvents events;
};

const ScreenEvents WALK{ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents BOX{ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents WHITE{ScreenEvent::FADE_WHITE};
const ScreenEvents BLACK{ScreenEvent::FADE_BLACK};
const ScreenEvents BATTLE{ScreenEvent::BATTLE_STARTED};
const ScreenEvents BANNER{ScreenEvent::LOCATION_BANNER, ScreenEvent::PLAYER_CAN_MOVE};

//  A look at `frames`, judged up to `until_ms`.
ScreenLook look_of(const std::vector<Frame>& frames, int until_ms){
    ScreenLook look;
    for (const Frame& frame : frames){
        look.log.record(frame.events, T0 + Milliseconds(frame.ms));
    }
    look.window = {T0, T0 + Milliseconds(until_ms)};
    return look;
}

ScreenLook quiet(){
    return look_of({{0, WALK}, {300, WALK}}, 300);
}
ScreenLook dialog(){
    return look_of({{0, WALK}, {100, BOX}}, 300);
}
ScreenLook wild(){
    return look_of({{0, WALK}, {100, WHITE}, {400, WALK}, {1500, BATTLE}}, 2000);
}
ScreenLook trainer(){
    return look_of({{0, WALK}, {100, BOX}, {2000, BLACK}, {2500, BATTLE}}, 3000);
}
ScreenLook warp(){
    return look_of({{0, WALK}, {100, BLACK}, {600, WALK}, {900, BANNER}}, 1200);
}
//  A white fade with no battle yet: an encounter's flash or a cave exit.
ScreenLook white_unsettled(){
    return look_of({{0, WALK}, {100, WHITE}, {700, WALK}}, 1000);
}
//  A trainer's lines, then the battle's black, still down: no battle yet.
ScreenLook trainer_opening(){
    return look_of({{0, WALK}, {100, BOX}, {2000, BLACK}}, 2100);
}
//  A door's black, back up with no box before it: a warp, not yet settled.
ScreenLook black_unsettled(){
    return look_of({{0, WALK}, {100, BLACK}, {700, WALK}}, 1000);
}
//  The same white, looked at for long enough after it: a cave exit.
ScreenLook white_settled(){
    return look_of({{0, WALK}, {100, WHITE}, {700, WALK}, {10000, WALK}}, 10000);
}


//  The fake game: the screen its looks show, one per look and then the
//  last again, and a trace of what was looked at, cleared and run.
struct FakeGame{
    std::vector<ScreenLook> screens;
    size_t looks = 0;
    std::vector<std::string> trace;

    ScreenLook look(bool fresh){
        trace.push_back(fresh ? "look" : "look again");
        const ScreenLook& out = screens.empty() ? quiet() : screens[std::min(looks, screens.size() - 1)];
        looks++;
        return out;
    }
};

//  A handler that notes its kind; fails with `failure` if set.
InterruptHandler noting(FakeGame& game, std::string name, InterruptPose pose = InterruptPose::UNCHANGED, std::string failure = ""){
    return InterruptHandler{
        .handle = [&game, name, failure](RunState&, const InterruptVerdict&){
            game.trace.push_back("clear " + name);
            return failure.empty() ? outcome_ok() : outcome_failed(failure);
        },
        .pose = pose,
    };
}

//  All four handlers, a warp losing the pose; MASH starts on a dialog.
StoryInterrupts fake_interrupts(FakeGame& game){
    StoryInterrupts interrupts;
    interrupts.look = [&game](bool fresh){
        return game.look(fresh);
    };
    interrupts.handlers.add(InterruptKind::WILD_ENCOUNTER, noting(game, "wild"));
    interrupts.handlers.add(InterruptKind::TRAINER_SIGHT, noting(game, "trainer"));
    interrupts.handlers.add(InterruptKind::DIALOG, noting(game, "dialog"));
    interrupts.handlers.add(InterruptKind::WARP, noting(game, "warp", InterruptPose::LOST));
    interrupts.handlers.starts_on(StoryStepKind::MASH, InterruptKind::DIALOG);
    return interrupts;
}

StoryStep make(StoryStepKind kind, std::string name){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    return step;
}

//  The step's action: notes it ran, and fails its first `failures` runs,
//  returning a failed Outcome or, with `throws`, throwing.
struct FakeAction{
    size_t failures = 0;
    bool throws = false;
};
StoryActionTable table_of(FakeGame& game, FakeAction action){
    StoryActionTable table;
    auto runs = std::make_shared<size_t>(0);
    table.add_any_kind("act", StoryAction{
        .act = [&game, action, runs](RunState&, const StoryStep& step) -> Outcome{
            game.trace.push_back("run " + step.name);
            if ((*runs)++ >= action.failures){
                return outcome_ok();
            }
            if (action.throws){
                throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "stopped by a dialog");
            }
            return outcome_failed("dialog", "stopped by a dialog");
        },
    });
    return table;
}

//  A guard that notes the interrupts it was told of.
class NotingGuard : public StepGuard{
public:
    explicit NotingGuard(FakeGame& game)
        : m_game(game)
    {}
    virtual void on_interrupt(const StoryStep&, const InterruptCheck& check) override{
        m_game.trace.push_back(std::format("told {} {}", interrupt_kind_name(check.verdict.kind), to_string(check.outcome)));
    }

private:
    FakeGame& m_game;
};

//  `step` (variant "act") run once by a TableExecutor with the fake
//  game's interrupts, from a known pose.
struct FakeRun{
    Outcome outcome;
    RunState state;
};
FakeRun run_step(FakeGame& game, StoryInterrupts& interrupts, StoryStep step, FakeAction action = {}){
    const StoryActionTable table = table_of(game, action);
    NotingGuard guard(game);
    TableExecutor executor(table, guard);
    executor.set_interrupts(&interrupts);
    FakeRun out;
    out.state.pose.map = "Town";
    out.state.pose.certainty = PoseCertainty::TILE;
    step.variant = "act";
    out.outcome = executor.run(out.state, step);
    return out;
}

UnitTestResult expect_trace(const FakeGame& game, const std::vector<std::string>& expected){
    if (game.trace == expected){
        return true;
    }
    std::string out = "Expected the trace:";
    for (const std::string& line : expected){
        out += "\n  " + line;
    }
    out += "\ngot:";
    for (const std::string& line : game.trace){
        out += "\n  " + line;
    }
    return out;
}


UnitTestResult test_no_interrupts_set(){
    FakeGame game;
    const StoryActionTable table = table_of(game, {});
    StepGuard guard;
    TableExecutor executor(table, guard);
    RunState state;
    StoryStep step = make(StoryStepKind::ROUTE, "walk");
    step.variant = "act";
    const Outcome outcome = executor.run(state, step);
    if (!outcome.succeeded()){
        return "Expected the step to succeed, got " + to_string(outcome);
    }
    return expect_trace(game, {"run walk"});
}

UnitTestResult test_quiet_screen(){
    FakeGame game;
    StoryInterrupts interrupts = fake_interrupts(game);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"));
    if (!run.outcome.succeeded() || run.state.pose.certainty != PoseCertainty::TILE){
        return "Expected the step to succeed where it stood, got " + to_string(run.outcome);
    }
    return expect_trace(game, {"look", "run walk"});
}

//  A look showing `screen` before a ROUTE step: cleared by `handler`, then
//  the step runs.
UnitTestResult test_cleared(ScreenLook screen, const std::string& kind, const std::string& handler, PoseCertainty pose){
    FakeGame game;
    game.screens = {std::move(screen), quiet()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"));
    if (!run.outcome.succeeded()){
        return "Expected the step to succeed, got " + to_string(run.outcome);
    }
    if (run.state.pose.certainty != pose){
        return std::string("Expected the pose ") + certainty_name(pose) + ", got " + certainty_name(run.state.pose.certainty);
    }
    return expect_trace(game, {"look", "clear " + handler, "told " + kind + " ", "run walk"});
}

UnitTestResult test_wild_encounter(){
    return test_cleared(wild(), "wild encounter", "wild", PoseCertainty::TILE);
}
UnitTestResult test_trainer_sight(){
    return test_cleared(trainer(), "trainer sight", "trainer", PoseCertainty::TILE);
}
UnitTestResult test_dialog(){
    return test_cleared(dialog(), "dialog", "dialog", PoseCertainty::TILE);
}
UnitTestResult test_warp(){
    return test_cleared(warp(), "warp", "warp", PoseCertainty::UNKNOWN);
}

UnitTestResult test_step_starts_on_it(){
    FakeGame game;
    game.screens = {dialog()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::MASH, "scene"));
    if (!run.outcome.succeeded()){
        return "Expected the step to succeed, got " + to_string(run.outcome);
    }
    return expect_trace(game, {"look", "run scene"});
}

UnitTestResult test_handler_fails_the_step(){
    FakeGame game;
    game.screens = {dialog()};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.handlers = InterruptHandlerTable();
    interrupts.handlers.add(InterruptKind::DIALOG, noting(game, "dialog", InterruptPose::UNCHANGED, "still_up"));
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"));
    if (run.outcome.failure != "interrupt_failed" || run.outcome.detail != "dialog: still_up"){
        return "Expected interrupt_failed: dialog: still_up, got " + to_string(run.outcome);
    }
    return expect_trace(game, {"look", "clear dialog", "told dialog interrupt_failed: dialog: still_up"});
}

UnitTestResult test_unhandled_kind(){
    FakeGame game;
    game.screens = {trainer()};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.handlers = InterruptHandlerTable();
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"));
    if (run.outcome.failure != "unhandled_interrupt"){
        return "Expected unhandled_interrupt, got " + to_string(run.outcome);
    }
    return expect_trace(game, {"look", "told trainer sight unhandled_interrupt: trainer sight before walk"});
}

UnitTestResult test_unsettled_looks_again(){
    FakeGame game;
    game.screens = {white_unsettled(), white_settled(), quiet()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"));
    if (!run.outcome.succeeded()){
        return "Expected the step to succeed, got " + to_string(run.outcome);
    }
    return expect_trace(game, {"look", "look again", "clear warp", "told warp ", "run walk"});
}

//  A door's fade is no battle's opening: three looks, as before.
UnitTestResult test_unsettled_is_bounded(){
    FakeGame game;
    game.screens = {black_unsettled()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"));
    if (run.outcome.failure != "interrupt_unsettled"){
        return "Expected interrupt_unsettled, got " + to_string(run.outcome);
    }
    return expect_trace(game, {"look", "look again", "look again", "told unsettled interrupt_unsettled: walk"});
}

//  A battle's opening still playing when a step that starts on a battle
//  begins (BDSP's Starly battle, right after the starter is picked): the
//  step's, on the first look, not interrupt_unsettled.
UnitTestResult test_unsettled_before_a_battle(){
    FakeGame game;
    game.screens = {white_unsettled()};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.handlers.starts_on(StoryStepKind::BATTLE, InterruptKind::WILD_ENCOUNTER);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::BATTLE, "starly"));
    if (!run.outcome.succeeded()){
        return "Expected the battle step to run, got " + to_string(run.outcome);
    }
    return expect_trace(game, {"look", "run starly"});
}

//  A battle's opening still playing when a walk step begins: the walk
//  before it ended in grass or in a trainer's sight (BDSP fault sweep, 51
//  steps of 00.01-01.11). The looks wait for the battle, past max_looks,
//  and its handler clears it before the step.
UnitTestResult test_opening_before_a_walk(ScreenLook opening, ScreenLook battle, const std::string& kind, const std::string& handler){
    FakeGame game;
    game.screens = {opening, opening, opening, opening, std::move(battle), quiet()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"));
    if (!run.outcome.succeeded()){
        return "Expected the walk to run after the battle, got " + to_string(run.outcome);
    }
    return expect_trace(game, {
        "look", "look again", "look again", "look again", "look again",
        "clear " + handler, "told " + kind + " ", "run walk",
    });
}
UnitTestResult test_wild_opening_before_a_walk(){
    return test_opening_before_a_walk(white_unsettled(), wild(), "wild encounter", "wild");
}
UnitTestResult test_trainer_opening_before_a_walk(){
    return test_opening_before_a_walk(trainer_opening(), trainer(), "trainer sight", "trainer");
}

//  A white that shows no battle and does not settle: max_opening_looks.
UnitTestResult test_opening_is_bounded(){
    FakeGame game;
    game.screens = {white_unsettled()};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.max_opening_looks = 5;
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"));
    if (run.outcome.failure != "interrupt_unsettled"){
        return "Expected interrupt_unsettled, got " + to_string(run.outcome);
    }
    return expect_trace(game, {
        "look", "look again", "look again", "look again", "look again",
        "told unsettled interrupt_unsettled: walk",
    });
}

UnitTestResult test_failure_explained(bool throws){
    FakeGame game;
    //  Before the step, after its failure, before its second run.
    game.screens = {quiet(), dialog(), quiet()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"), {.failures = 1, .throws = throws});
    if (!run.outcome.succeeded()){
        return "Expected the step run again and succeed, got " + to_string(run.outcome);
    }
    return expect_trace(game, {"look", "run walk", "look", "clear dialog", "told dialog ", "look", "run walk"});
}
UnitTestResult test_failed_outcome_explained(){
    return test_failure_explained(false);
}
UnitTestResult test_thrown_failure_explained(){
    return test_failure_explained(true);
}

UnitTestResult test_failure_not_explained(){
    FakeGame game;
    StoryInterrupts interrupts = fake_interrupts(game);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"), {.failures = 1});
    if (run.outcome.failure != "dialog"){
        return "Expected the step's own failure, got " + to_string(run.outcome);
    }
    return expect_trace(game, {"look", "run walk", "look"});
}

//  A step that keeps failing with a dialog that keeps coming back: run
//  again while the dialogs are cleared, until there have been too many.
UnitTestResult test_too_many_interrupts(){
    FakeGame game;
    game.screens = {dialog()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const FakeRun run = run_step(game, interrupts, make(StoryStepKind::ROUTE, "walk"), {.failures = 10});
    if (run.outcome.failure != "dialog" || game.looks != interrupts.max_per_step + 1){
        return std::format("Expected the step's own failure after {} looks, got {} after {}",
            interrupts.max_per_step + 1, to_string(run.outcome), game.looks);
    }
    if (game.trace.back() != "told dialog too_many_interrupts: walk"){
        return "Expected the guard told of too many interrupts, got " + game.trace.back();
    }
    return true;
}

UnitTestResult test_table_rejects(){
    InterruptHandlerTable table;
    table.add(InterruptKind::DIALOG, {});
    for (InterruptKind kind : {InterruptKind::NONE, InterruptKind::UNSETTLED, InterruptKind::DIALOG}){
        try{
            table.add(kind, {});
            return std::string("Expected a handler for ") + interrupt_kind_name(kind) + " rejected.";
        }catch (InternalProgramError&){}
    }
    if (table.find(InterruptKind::DIALOG) == nullptr || table.find(InterruptKind::WARP) != nullptr){
        return "Expected the dialog's handler found and no warp's.";
    }
    return true;
}

//  A guard that notes how the screen looked before each step.
class ScreenGuard : public StepGuard{
public:
    explicit ScreenGuard(FakeGame& game)
        : m_game(game)
    {}
    virtual void on_screen(const StoryStep& step, const ScreenSettle& screen) override{
        m_game.trace.push_back("before " + step.name + ": " + describe(screen));
    }

private:
    FakeGame& m_game;
};

//  The guard hears how the screen looked before every step, interrupt or
//  not: the run log's answer to "had the scene ended before the input?".
UnitTestResult test_screen_before_each_step(){
    FakeGame game;
    //  A scene's closing fade still down: no battle comes, so the step
    //  fails unsettled, and the guard hears the fade was showing.
    game.screens = {look_of({{0, WALK}, {100, BLACK}}, 300)};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.max_opening_looks = 2;
    const StoryActionTable table = table_of(game, {});
    ScreenGuard guard(game);
    TableExecutor executor(table, guard);
    executor.set_interrupts(&interrupts);
    RunState state;
    StoryStep step = make(StoryStepKind::ROUTE, "by_the_counter");
    step.variant = "act";
    const Outcome faded = executor.run(state, step);
    if (faded.succeeded()){
        return "Expected a step in a fade to fail unsettled, got " + to_string(faded);
    }
    game.screens = {quiet()};
    game.looks = 0;
    const Outcome settled = executor.run(state, step);
    if (!settled.succeeded()){
        return "Expected the step on a quiet screen to run, got " + to_string(settled);
    }
    return expect_trace(game, {
        "look", "look again", "before by_the_counter: black fade showing (quiet 0.2 s)",
        "look", "before by_the_counter: settled overworld (quiet 0.3 s)", "run by_the_counter",
    });
}

UnitTestResult test_settle_of_a_look(){
    struct Row{
        const char* name;
        ScreenLook look;
        const char* expected;
    };
    const std::vector<Row> rows = {
        {"no frame", ScreenLook{}, "no frame seen"},
        {"a quiet overworld", quiet(), "settled overworld (quiet 0.3 s)"},
        {"a box", dialog(), "dialog box, player can move showing (quiet 0.2 s)"},
        {"a door's fade back up", black_unsettled(), "settled overworld (quiet 0.3 s)"},
    };
    for (const Row& row : rows){
        const std::string got = describe(judge_settle(row.look));
        if (got != row.expected){
            return std::format("{}: expected \"{}\", got \"{}\"", row.name, row.expected, got);
        }
    }
    return true;
}


struct StoryInterruptsInput{
    std::string_view name;
    UnitTestResult (*test)();
};

struct StoryInterruptsExpectation{
    std::string_view description;
};

struct StoryInterruptsCase{
    StoryInterruptsInput input;
    StoryInterruptsExpectation expectation;
};

class Test_StoryInterrupts : public UnitTest{
public:
    explicit Test_StoryInterrupts(StoryInterruptsCase scenario)
        : UnitTest("Pokemon::StoryInterrupts - " + std::string(scenario.input.name))
        , m_scenario(scenario)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.test();
    }

private:
    StoryInterruptsCase m_scenario;
};

const StoryInterruptsCase STORY_INTERRUPTS_CASES[] = {
    {
        .input = {.name = "no interrupts set: no look", .test = test_no_interrupts_set},
        .expectation = {.description = "the executor runs the step as before"},
    },
    {
        .input = {.name = "a quiet screen: one look, then the step", .test = test_quiet_screen},
        .expectation = {.description = "nothing cleared, the pose kept"},
    },
    {
        .input = {.name = "a wild encounter before a step", .test = test_wild_encounter},
        .expectation = {.description = "the wild handler, then the step"},
    },
    {
        .input = {.name = "a trainer's sight before a step", .test = test_trainer_sight},
        .expectation = {.description = "the trainer handler, then the step"},
    },
    {
        .input = {.name = "a dialog before a step", .test = test_dialog},
        .expectation = {.description = "the dialog handler, then the step"},
    },
    {
        .input = {.name = "a warp before a step", .test = test_warp},
        .expectation = {.description = "the warp handler, the pose lost, then the step"},
    },
    {
        .input = {.name = "a step that starts on it", .test = test_step_starts_on_it},
        .expectation = {.description = "a MASH's box is the step's, not an interrupt"},
    },
    {
        .input = {.name = "a handler that fails fails the step", .test = test_handler_fails_the_step},
        .expectation = {.description = "interrupt_failed, the step not run"},
    },
    {
        .input = {.name = "a kind with no handler fails the step", .test = test_unhandled_kind},
        .expectation = {.description = "unhandled_interrupt, the step not run"},
    },
    {
        .input = {.name = "unsettled: look again until it settles", .test = test_unsettled_looks_again},
        .expectation = {.description = "a white with no battle after it is a warp"},
    },
    {
        .input = {.name = "unsettled: at most max_looks looks", .test = test_unsettled_is_bounded},
        .expectation = {.description = "a door's fade: interrupt_unsettled after three looks"},
    },
    {
        .input = {.name = "unsettled: a battle's opening before a battle step", .test = test_unsettled_before_a_battle},
        .expectation = {.description = "the step's, after one look"},
    },
    {
        .input = {.name = "unsettled: a wild battle's opening before a walk", .test = test_wild_opening_before_a_walk},
        .expectation = {.description = "waits for the battle, clears it, then the walk"},
    },
    {
        .input = {.name = "unsettled: a trainer battle's opening before a walk", .test = test_trainer_opening_before_a_walk},
        .expectation = {.description = "waits for the battle, fights it, then the walk"},
    },
    {
        .input = {.name = "unsettled: at most max_opening_looks for an opening", .test = test_opening_is_bounded},
        .expectation = {.description = "a white with no battle: interrupt_unsettled"},
    },
    {
        .input = {.name = "a failed step an interrupt explains runs again", .test = test_failed_outcome_explained},
        .expectation = {.description = "cleared after the failure, then the step again"},
    },
    {
        .input = {.name = "a thrown step an interrupt explains runs again", .test = test_thrown_failure_explained},
        .expectation = {.description = "the console's failure too"},
    },
    {
        .input = {.name = "a failed step with nothing on screen fails", .test = test_failure_not_explained},
        .expectation = {.description = "the step's own failure, run once"},
    },
    {
        .input = {.name = "too many interrupts fail the step", .test = test_too_many_interrupts},
        .expectation = {.description = "the step's failure once max_per_step are cleared"},
    },
    {
        .input = {.name = "table: one handler a kind that can be cleared", .test = test_table_rejects},
        .expectation = {.description = "rejects NONE, UNSETTLED and a second handler"},
    },
    {
        .input = {.name = "the screen before each step, for the log", .test = test_screen_before_each_step},
        .expectation = {.description = "a fade still down is told, and fails unsettled; a quiet overworld is settled"},
    },
    {
        .input = {.name = "how a look ended", .test = test_settle_of_a_look},
        .expectation = {.description = "the last frame's events and how long they held"},
    },
};

} // namespace


void add_tests_StoryInterrupts(UnitTestDatabase& database){
    for (const StoryInterruptsCase& scenario : STORY_INTERRUPTS_CASES){
        database.add<Test_StoryInterrupts>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
