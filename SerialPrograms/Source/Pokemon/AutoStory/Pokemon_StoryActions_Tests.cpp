/*  Story Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_StoryActions.h"
#include "Pokemon_AutoStory_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

StoryStep make(StoryStepKind kind, std::string name, std::string action = ""){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.action = std::move(action);
    return step;
}

//  An action that fails with `name`, so a test sees which one ran.
StoryAction named(std::string name){
    return StoryAction{
        .act = [name](RunState&, const StoryStep&){
            return outcome_failed(name);
        },
    };
}

//  The name of the action the table finds for `kind` and `action`; "none"
//  without one.
std::string found(const StoryActionTable& table, StoryStepKind kind, std::string name){
    const StoryAction* action = table.find(make(kind, "step", std::move(name)));
    if (action == nullptr){
        return "none";
    }
    RunState state;
    return action->act(state, make(kind, "step")).failure;
}


UnitTestResult test_lookup_order(){
    StoryActionTable table;
    table.add(StoryStepKind::TALK, "", named("talk default"));
    table.add(StoryStepKind::TALK, "bill", named("talk bill"));
    table.add_any_kind("bill", named("any bill"));
    table.add_any_kind("nurse", named("any nurse"));
    const std::pair<std::string, std::string> checks[] = {
        {found(table, StoryStepKind::TALK, "bill x=1"), "talk bill"},
        {found(table, StoryStepKind::MASH, "bill"), "any bill"},
        {found(table, StoryStepKind::TALK, "nurse"), "any nurse"},
        {found(table, StoryStepKind::TALK, "oak"), "talk default"},
        {found(table, StoryStepKind::TALK, ""), "talk default"},
        {found(table, StoryStepKind::TALK, "x=1"), "talk default"},
        {found(table, StoryStepKind::MASH, "oak"), "none"},
        {found(table, StoryStepKind::MASH, ""), "none"},
    };
    for (const auto& [actual, expected] : checks){
        if (actual != expected){
            return "Expected " + expected + ", got " + actual;
        }
    }
    return true;
}

UnitTestResult test_duplicate_actions_throw(){
    StoryActionTable table;
    table.add(StoryStepKind::TALK, "bill", named("first"));
    table.add_any_kind("bill", named("any"));
    const auto throws = [](auto add){
        try{
            add();
        }catch (InternalProgramError&){
            return true;
        }
        return false;
    };
    if (!throws([&]{ table.add(StoryStepKind::TALK, "bill", named("second")); })){
        return "Expected a second TALK 'bill' to throw.";
    }
    if (!throws([&]{ table.add_any_kind("bill", named("second")); })){
        return "Expected a second any-kind 'bill' to throw.";
    }
    if (!throws([&]{ table.add_any_kind("", named("nameless")); })){
        return "Expected an any-kind action without a name to throw.";
    }
    return found(table, StoryStepKind::TALK, "bill") == "first" ? UnitTestResult(true) : UnitTestResult("Expected the first action kept.");
}

//  The args a test Mart takes.
const std::vector<StoryArgSpec> MART_ARGS = {
    {.key = "map", .type = StoryArgType::WORD, .required = true, .fallback = std::nullopt, .why = ""},
    {.key = "budget", .type = StoryArgType::INTEGER, .required = true, .fallback = std::nullopt, .why = ""},
};

//  `step` with `args`.
StoryStep with_args(StoryStep step, StoryActionArgs args){
    step.args = std::move(args);
    return step;
}

UnitTestResult test_lint_names_steps_without_actions(){
    StoryActionTable table;
    table.add(StoryStepKind::ROUTE, "", named("route"));
    table.add_any_kind("bill", named("bill"));
    StoryAction mart = named("mart");
    mart.args = MART_ARGS;
    table.add(StoryStepKind::GAME, "mart", std::move(mart));
    const StoryCheckpoint data{
        .id = "00.05",
        .title = "",
        .start = std::nullopt,
        .steps = {
            make(StoryStepKind::ROUTE, "to the cape", "route"),
            make(StoryStepKind::GAME, "meet Bill", "bill"),
            make(StoryStepKind::GAME, "teleporter", "cell_separator"),
            make(StoryStepKind::TALK, "talk"),
            with_args(make(StoryStepKind::GAME, "buy", "mart"), {{"map", std::string("M")}, {"budget", int64_t(1500)}}),
            with_args(make(StoryStepKind::GAME, "buy more", "mart"), {{"map", std::string("M")}}),
            with_args(make(StoryStepKind::GAME, "meet Bill again", "bill"), {{"slot", int64_t(1)}}),
        },
    };
    const std::vector<std::string> problems = lint_story_checkpoint(data, table);
    const std::vector<std::string> expected = {
        "00.05 step 3 teleporter: no action for GAME 'cell_separator'",
        "00.05 step 4 talk: no action for TALK ''",
        "00.05 step 6 buy more: missing argument \"budget\" for GAME 'mart'",
        "00.05 step 7 meet Bill again: unknown argument \"slot\" for GAME 'bill'",
    };
    if (problems != expected){
        std::string got;
        for (const std::string& problem : problems){
            got += "\n" + problem;
        }
        return "Expected two steps without actions and two with bad args, got:" + got;
    }
    return true;
}


//  A binding that does TALK itself when its table has no action for it.
class FakeTableExecutor : public TableExecutor{
public:
    using TableExecutor::TableExecutor;

protected:
    virtual Outcome run_action(RunState& state, const StoryStep& step) override{
        const Outcome outcome = TableExecutor::run_action(state, step);
        if (step.kind == StoryStepKind::TALK && outcome.failure == "unsupported_step"){
            return outcome_failed("talk binding");
        }
        return outcome;
    }
};

//  Writes down what the executor asked of it.
class FakeGuard : public StepGuard{
public:
    std::vector<std::string> trace;
    bool stops = false;

    virtual void before_step(const StoryStep& step) override{
        trace.push_back("before " + step.name);
    }
    virtual bool stops_on_failure(const StoryStep&) const override{
        return stops;
    }
    virtual void on_failure(const StoryStep& step, const OperationFailedException& error) override{
        trace.push_back("failed " + step.name + ": " + error.message());
    }
};

StoryAction throwing(){
    return StoryAction{
        .act = [](RunState&, const StoryStep&) -> Outcome{
            throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "no dialog");
        },
        .needs_console = true,
    };
}

UnitTestResult test_executor_uses_table_then_binding(){
    StoryActionTable table;
    table.add(StoryStepKind::GAME, "bill", named("bill action"));
    FakeGuard guard;
    FakeTableExecutor executor(table, guard);
    RunState state;
    const std::string bill = executor.run(state, make(StoryStepKind::GAME, "bill", "bill")).failure;
    const std::string talk = executor.run(state, make(StoryStepKind::TALK, "talk")).failure;
    const std::string mash = executor.run(state, make(StoryStepKind::MASH, "mash")).failure;
    if (bill != "bill action" || talk != "talk binding" || mash != "unsupported_step"){
        return "Expected table, binding, unsupported; got " + bill + ", " + talk + ", " + mash;
    }
    if (guard.trace != std::vector<std::string>{"before bill", "before talk", "before mash"}){
        return "Expected the guard before every step.";
    }
    return true;
}

UnitTestResult test_guard_turns_failures_into_outcomes(){
    StoryActionTable table;
    table.add(StoryStepKind::TALK, "", throwing());
    FakeGuard guard;
    TableExecutor executor(table, guard);
    RunState state;
    const Outcome outcome = executor.run(state, make(StoryStepKind::TALK, "oak"));
    if (outcome.failure != "operation_failed" || outcome.detail != "no dialog"){
        return "Expected operation_failed: no dialog, got " + to_string(outcome);
    }
    if (guard.trace != std::vector<std::string>{"before oak", "failed oak: no dialog"}){
        return "Expected the guard told of the failure.";
    }
    return true;
}

UnitTestResult test_guard_stop_rethrows(){
    StoryActionTable table;
    table.add(StoryStepKind::TALK, "", throwing());
    table.add(StoryStepKind::MASH, "", StoryAction{
        .act = [](RunState&, const StoryStep&) -> Outcome{
            throw std::runtime_error("not a console failure");
        },
    });
    FakeGuard guard;
    guard.stops = true;
    TableExecutor executor(table, guard);
    RunState state;
    try{
        executor.run(state, make(StoryStepKind::TALK, "oak"));
        return "Expected a stop row's failure rethrown.";
    }catch (OperationFailedException&){}
    guard.stops = false;
    try{
        executor.run(state, make(StoryStepKind::MASH, "scene"));
        return "Expected other exceptions passed through.";
    }catch (std::runtime_error&){}
    if (guard.trace != std::vector<std::string>{"before oak", "before scene"}){
        return "Expected no failure reported for a stop or another exception.";
    }
    return true;
}


struct StoryActionsInput{
    std::string_view name;
    UnitTestResult (*test)();
};

struct StoryActionsExpectation{
    std::string_view description;
};

struct StoryActionsCase{
    StoryActionsInput input;
    StoryActionsExpectation expectation;
};

class Test_StoryActions : public UnitTest{
public:
    explicit Test_StoryActions(StoryActionsCase scenario)
        : UnitTest("Pokemon::StoryActions - " + std::string(scenario.input.name))
        , m_scenario(scenario)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.test();
    }

private:
    StoryActionsCase m_scenario;
};

const StoryActionsCase STORY_ACTIONS_CASES[] = {
    {
        .input = {.name = "table: exact, then any kind, then the kind's default", .test = test_lookup_order},
        .expectation = {.description = "finds the most specific action, or none"},
    },
    {
        .input = {.name = "table: a second action for a key throws", .test = test_duplicate_actions_throw},
        .expectation = {.description = "keeps the first action and rejects nameless any-kind actions"},
    },
    {
        .input = {.name = "lint: steps without an action", .test = test_lint_names_steps_without_actions},
        .expectation = {.description = "names each step the table has no action for"},
    },
    {
        .input = {.name = "executor: the table, then the binding", .test = test_executor_uses_table_then_binding},
        .expectation = {.description = "a step without a table action is the binding's or unsupported, guarded before every step"},
    },
    {
        .input = {.name = "executor: a console failure fails the step", .test = test_guard_turns_failures_into_outcomes},
        .expectation = {.description = "turns OperationFailedException into operation_failed and tells the guard"},
    },
    {
        .input = {.name = "executor: a stop row rethrows", .test = test_guard_stop_rethrows},
        .expectation = {.description = "rethrows on a stop row and passes other exceptions through"},
    },
};

} // namespace


void add_tests_StoryActions(UnitTestDatabase& database){
    for (const StoryActionsCase& scenario : STORY_ACTIONS_CASES){
        database.add<Test_StoryActions>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
