/*  Story RNG Handoff Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_StoryRngHandoff.h"
#include "Pokemon_StoryRngHandoff_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

struct RngHandoffInput{
    std::string_view name;
    UnitTestResult (*body)(Logger& logger);
};

struct RngHandoffExpectation{
    std::string_view description;
};

struct RngHandoffTestCase{
    RngHandoffInput input;
    RngHandoffExpectation expectation;
};

class Test_RngHandoff : public UnitTest{
public:
    explicit Test_RngHandoff(RngHandoffTestCase scenario)
        : UnitTest("Pokemon::StoryRngHandoff - " + std::string(scenario.input.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_scenario.input.body(logger);
    }

private:
    RngHandoffTestCase m_scenario;
};

//  rng_handoff_mode()'s two options.
UnitTestResult test_mode(Logger&){
    struct Case{
        RngHandoffOptions options;
        RngHandoffMode expected;
    };
    const Case cases[] = {
        {.options = {.stop = false, .run = false}, .expected = RngHandoffMode::OFF},
        {.options = {.stop = true, .run = false}, .expected = RngHandoffMode::STOP},
        {.options = {.stop = false, .run = true}, .expected = RngHandoffMode::RUN},
        {.options = {.stop = true, .run = true}, .expected = RngHandoffMode::RUN},
    };
    for (const Case& item : cases){
        if (rng_handoff_mode(item.options) != item.expected){
            return std::format("stop={}, run={}: wrong mode", item.options.stop, item.options.run);
        }
    }
    return true;
}

//  A success is an ok Outcome and calls the program once.
UnitTestResult test_success(Logger& logger){
    int calls = 0;
    const Outcome outcome = run_rng_handoff(logger, "Test RNG", [&]{
        calls++;
        return RngHandoffResult{.summary = "2 resets"};
    });
    if (!outcome.succeeded()){
        return "Expected an ok outcome";
    }
    if (calls != 1){
        return std::format("Expected 1 call, got {}", calls);
    }
    return true;
}

//  A failure throws, once: nothing reruns the program.
UnitTestResult test_failure(Logger& logger){
    int calls = 0;
    try{
        run_rng_handoff(logger, "Test RNG", [&]{
            calls++;
            return RngHandoffResult{.failure = "out of resets"};
        });
    }catch (OperationFailedException&){
        if (calls != 1){
            return std::format("Expected 1 call, got {}", calls);
        }
        return true;
    }
    return "Expected OperationFailedException";
}

//  The user's turn is returned, not thrown, once; a success has none; a
//  failure throws even with one.
UnitTestResult test_user_turn(Logger& logger){
    int calls = 0;
    const std::optional<std::string> turn = run_rng_handoff_or_user_turn(logger, "Test RNG", [&]{
        calls++;
        return RngHandoffResult{.user_turn = "save by hand"};
    });
    if (!turn || *turn != "save by hand"){
        return "Expected the user's turn";
    }
    if (calls != 1){
        return std::format("Expected 1 call, got {}", calls);
    }
    const std::optional<std::string> none = run_rng_handoff_or_user_turn(logger, "Test RNG", []{
        return RngHandoffResult{.summary = "shiny"};
    });
    if (none){
        return "A success has no user's turn";
    }
    try{
        run_rng_handoff_or_user_turn(logger, "Test RNG", []{
            return RngHandoffResult{.failure = "out of resets", .user_turn = "ignored"};
        });
    }catch (OperationFailedException&){
        return true;
    }
    return "A failure must throw even with a user's turn";
}

//  A stop reports its message, then each line, to the log and to `show`;
//  the notification text is one line each.
UnitTestResult test_stop_report(Logger& logger){
    std::vector<std::string> shown;
    const auto show = [&](const std::string& line){
        shown.push_back(line);
    };
    const std::string text = report_rng_handoff_stop(logger, "Stopped for RNG.", {"Attempt 1", "Attempt 2"}, show);
    if (text != "Stopped for RNG.\nAttempt 1\nAttempt 2"){
        return std::format("Wrong notification text: {}", text);
    }
    if (shown != std::vector<std::string>{"Stopped for RNG.", "Attempt 1", "Attempt 2"}){
        return "The overlay must get the message and then each line";
    }
    shown.clear();
    if (report_rng_handoff_stop(logger, "Stopped.", {}, show) != "Stopped." || shown.size() != 1){
        return "A stop without lines is its message alone";
    }
    return true;
}

const RngHandoffTestCase RNG_HANDOFF_CASES[] = {
    {
        .input = {.name = "RUN takes precedence over STOP", .body = test_mode},
        .expectation = {.description = "RUN wins when both options are on"},
    },
    {
        .input = {.name = "a stop reports its message and lines", .body = test_stop_report},
        .expectation = {.description = "log, overlay and notification text agree"},
    },
    {
        .input = {.name = "a success is ok", .body = test_success},
        .expectation = {.description = "an ok outcome after one call"},
    },
    {
        .input = {.name = "a failure throws without a retry", .body = test_failure},
        .expectation = {.description = "OperationFailedException after one call"},
    },
    {
        .input = {.name = "a user's turn is returned, not an error", .body = test_user_turn},
        .expectation = {.description = "the message comes back; a failure still throws"},
    },
};

} // namespace


void add_tests_StoryRngHandoff(UnitTestDatabase& database){
    for (const RngHandoffTestCase& scenario : RNG_HANDOFF_CASES){
        database.add<Test_RngHandoff>(scenario);
    }
}


} // namespace Pokemon
} // namespace PokemonAutomation
