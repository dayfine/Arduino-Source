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
#include "Pokemon_StoryVariant.h"
#include "Pokemon_AutoStory_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

std::string joined(const std::vector<std::string_view>& words){
    std::string out;
    for (std::string_view word : words){
        out += "[";
        out += word;
        out += "]";
    }
    return out;
}

StoryStep make(StoryStepKind kind, std::string name, std::string variant = ""){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.variant = std::move(variant);
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

//  The name of the action the table finds for `kind` and `variant`; "none"
//  without one.
std::string found(const StoryActionTable& table, StoryStepKind kind, std::string variant){
    const StoryAction* action = table.find(make(kind, "step", std::move(variant)));
    if (action == nullptr){
        return "none";
    }
    RunState state;
    return action->act(state, make(kind, "step")).failure;
}


UnitTestResult test_variant_words(){
    const std::string words = joined(variant_words("  mart  PewterCity_Mart budget=2000 "));
    if (words != "[mart][PewterCity_Mart][budget=2000]"){
        return "Expected three words, got " + words;
    }
    if (!variant_words("").empty() || !variant_words("   ").empty()){
        return "Expected no words in an empty variant.";
    }
    return true;
}

UnitTestResult test_variant_pairs(){
    const std::optional<VariantPair> pair = variant_pair("budget=2000");
    if (!pair || pair->key != "budget" || pair->value != "2000"){
        return "Expected budget=2000 as a pair.";
    }
    const std::optional<VariantPair> empty_value = variant_pair("door=");
    if (!empty_value || empty_value->key != "door" || !empty_value->value.empty()){
        return "Expected door= as a pair with an empty value.";
    }
    const std::optional<VariantPair> two = variant_pair("a=b=c");
    if (!two || two->key != "a" || two->value != "b=c"){
        return "Expected a=b=c split at the first '='.";
    }
    if (variant_pair("=2000") || variant_pair("mart")){
        return "Expected no pair without a key or an '='.";
    }
    return true;
}

UnitTestResult test_variant_name_and_values(){
    if (variant_name(" bill  shot=1") != "bill" || !variant_name("shot=1 bill").empty() || !variant_name("").empty()){
        return "Expected the name to be the first word unless it is a pair.";
    }
    const std::string_view variant = "mart potion=5 budget=2000 potion=9";
    if (variant_value(variant, "potion") != std::optional<std::string_view>("5")){
        return "Expected the first potion= value.";
    }
    if (variant_value(variant, "mart") || variant_value(variant, "repel")){
        return "Expected no value for a plain word or a missing key.";
    }
    return true;
}

UnitTestResult test_variant_numbers(){
    if (variant_number("2000") != 2000u || variant_number("0") != 0u || variant_number("999999") != 999999u){
        return "Expected decimal numbers read.";
    }
    if (variant_number("") || variant_number("12a") || variant_number("-1") || variant_number("1234567")){
        return "Expected no number from empty, non-digit or too long text.";
    }
    if (variant_number("123", 2) || variant_number("12", 2) != 12u){
        return "Expected max_digits respected.";
    }
    return true;
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

UnitTestResult test_lint_names_steps_without_actions(){
    StoryActionTable table;
    table.add(StoryStepKind::ROUTE, "", named("route"));
    table.add_any_kind("bill", named("bill"));
    const StoryCheckpoint data{
        .id = "00.05",
        .title = "",
        .start = std::nullopt,
        .steps = {
            make(StoryStepKind::ROUTE, "to the cape", "costs=grass"),
            make(StoryStepKind::GAME, "meet Bill", "bill"),
            make(StoryStepKind::GAME, "teleporter", "cell_separator x=1"),
            make(StoryStepKind::TALK, "talk"),
        },
    };
    const std::vector<std::string> problems = lint_story_checkpoint(data, table);
    const std::vector<std::string> expected = {
        "00.05 step 3 teleporter: no action for GAME 'cell_separator'",
        "00.05 step 4 talk: no action for TALK ''",
    };
    if (problems != expected){
        std::string got;
        for (const std::string& problem : problems){
            got += "\n" + problem;
        }
        return "Expected two steps without actions, got:" + got;
    }
    return true;
}


//  Steps without an action of their own go to this TALK.
class FakeTableExecutor : public TableExecutor{
public:
    using TableExecutor::TableExecutor;

protected:
    virtual Outcome talk(RunState&, const StoryStep&) override{
        return outcome_failed("talk virtual");
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

UnitTestResult test_executor_uses_table_then_virtuals(){
    StoryActionTable table;
    table.add(StoryStepKind::GAME, "bill", named("bill action"));
    FakeGuard guard;
    FakeTableExecutor executor(table, guard);
    RunState state;
    const std::string bill = executor.run(state, make(StoryStepKind::GAME, "bill", "bill")).failure;
    const std::string talk = executor.run(state, make(StoryStepKind::TALK, "talk")).failure;
    const std::string mash = executor.run(state, make(StoryStepKind::MASH, "mash")).failure;
    if (bill != "bill action" || talk != "talk virtual" || mash != "unsupported_step"){
        return "Expected table, virtual, unsupported; got " + bill + ", " + talk + ", " + mash;
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
        .input = {.name = "variant: words split on spaces", .test = test_variant_words},
        .expectation = {.description = "drops empty words between, before and after spaces"},
    },
    {
        .input = {.name = "variant: key=value pairs", .test = test_variant_pairs},
        .expectation = {.description = "splits at the first '=', needs a key, allows an empty value"},
    },
    {
        .input = {.name = "variant: name and values", .test = test_variant_name_and_values},
        .expectation = {.description = "names the first plain word and reads the first value of a key"},
    },
    {
        .input = {.name = "variant: numbers", .test = test_variant_numbers},
        .expectation = {.description = "reads decimal digits only, up to max_digits"},
    },
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
        .input = {.name = "executor: the table, then the kind's virtual", .test = test_executor_uses_table_then_virtuals},
        .expectation = {.description = "falls back to the per-kind virtuals, guarded before every step"},
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
