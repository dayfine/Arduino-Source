/*  Checkpoint Attempts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_CheckpointAttempts.h"
#include "Pokemon_CheckpointAttempts_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


struct AttemptsCase{
    std::string name;
    struct Input{
        size_t max_attempts = 1;
        //  Each attempt's result in order; past the end, FAILED.
        std::vector<AttemptResult> results;
        bool reset_fails = false;
    };
    Input input;
    struct Expectation{
        bool passed = false;
        size_t attempts = 0;
        //  The resets asked for, by next attempt: "2 3".
        std::string resets = {};
        //  The log, joined by " | ".
        std::string log = {};
    };
    Expectation expectation;
};

const AttemptsCase CASES[] = {
    {
        .name = "the first attempt passes",
        .input = {.max_attempts = 3, .results = {AttemptResult::PASSED}},
        .expectation = {.passed = true, .attempts = 1, .resets = "", .log = "[Checkpoint] 01.01: passed on attempt 1 (one-shot)"},
    },
    {
        .name = "the third attempt passes after two resets",
        .input = {.max_attempts = 3, .results = {AttemptResult::FAILED, AttemptResult::FAILED, AttemptResult::PASSED}},
        .expectation = {.passed = true, .attempts = 3, .resets = "2 3", .log = "[Checkpoint] 01.01: passed on attempt 3 (2 resets)"},
    },
    {
        .name = "every attempt fails",
        .input = {.max_attempts = 2, .results = {}},
        .expectation = {.passed = false, .attempts = 2, .resets = "2"},
    },
    {
        .name = "zero attempts runs one",
        .input = {.max_attempts = 0, .results = {}},
        .expectation = {.passed = false, .attempts = 1},
    },
    {
        .name = "a final failure ends the loop",
        .input = {.max_attempts = 3, .results = {AttemptResult::FINAL}},
        .expectation = {.passed = false, .attempts = 1},
    },
    {
        .name = "a failed reset ends the loop",
        .input = {.max_attempts = 3, .results = {}, .reset_fails = true},
        .expectation = {.passed = false, .attempts = 1, .resets = "2"},
    },
};

UnitTestResult check_case(const AttemptsCase& test){
    std::string resets;
    std::string log;
    const CheckpointAttempts attempts{
        .max_attempts = test.input.max_attempts,
        .run = [&](size_t attempt){
            return attempt <= test.input.results.size() ? test.input.results[attempt - 1] : AttemptResult::FAILED;
        },
        .reset = [&](size_t next_attempt){
            resets += std::format("{}{}", resets.empty() ? "" : " ", next_attempt);
            return !test.input.reset_fails;
        },
        .log = [&](const std::string& line){
            log += (log.empty() ? "" : " | ") + line;
        },
    };
    const CheckpointAttemptsResult result = run_checkpoint_attempts("01.01", attempts);
    if (result.passed != test.expectation.passed || result.attempts != test.expectation.attempts){
        return std::format(
            "expected passed {} after {}, got passed {} after {}",
            test.expectation.passed, test.expectation.attempts, result.passed, result.attempts
        );
    }
    if (resets != test.expectation.resets){
        return std::format("expected resets \"{}\", got \"{}\"", test.expectation.resets, resets);
    }
    if (log != test.expectation.log){
        return std::format("expected the log \"{}\", got \"{}\"", test.expectation.log, log);
    }
    return true;
}


class CheckpointAttemptsTest : public UnitTest{
public:
    explicit CheckpointAttemptsTest(AttemptsCase test_case)
        : UnitTest(std::format("Pokemon::CheckpointAttempts - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_case(m_case);
    }

private:
    AttemptsCase m_case;
};

} // namespace


void add_tests_CheckpointAttempts(UnitTestDatabase& database){
    for (const AttemptsCase& test : CASES){
        database.add<CheckpointAttemptsTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
