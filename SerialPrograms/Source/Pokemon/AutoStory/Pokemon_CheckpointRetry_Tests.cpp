/*  Checkpoint Retry Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_CheckpointRetry.h"
#include "Pokemon_CheckpointRetry_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  Thrown by the test's give_up, so the test can tell it from the attempt's.
struct GaveUp{};

//  A checkpoint whose first `failures` attempts fail.
struct CheckpointRetryInput{
    std::string name;
    CheckpointRetryPolicy policy;
    size_t failures;
};

struct CheckpointRetryExpectation{
    //  The attempt that passed; 0 if the loop gave up.
    size_t passed_on;
    //  The failure counts at which the loop reported.
    std::vector<size_t> reports;
    size_t resets;
    std::vector<std::string> log;
};

// One retry loop run and what it must do.
struct CheckpointRetryCase{
    CheckpointRetryInput input;
    CheckpointRetryExpectation expectation;
};

//  What the loop did through the test's actions.
struct CheckpointRetryRecord{
    size_t passed_on = 0;
    std::vector<size_t> reports;
    size_t resets = 0;
    std::vector<std::string> log;
};

CheckpointRetryRecord run_case(const CheckpointRetryInput& input, AutoStoryStats& stats){
    CheckpointRetryRecord record;
    size_t attempts = 0;
    CheckpointRetryActions actions{
        .run = [&](size_t attempt){
            attempts = attempt;
            if (attempt <= input.failures){
                throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "failed");
            }
        },
        .report = [&](const OperationFailedException&){
            record.reports.push_back(attempts);
        },
        .give_up = [](const OperationFailedException&){
            throw GaveUp();
        },
        .reset = [&](){
            record.resets++;
        },
        .update_stats = [](){},
        .log = [&](const std::string& line){
            record.log.push_back(line);
        },
    };
    try{
        record.passed_on = run_checkpoint_with_retries("01", input.policy, actions, stats);
    }catch (GaveUp&){
        record.passed_on = 0;
    }
    return record;
}

class Test_CheckpointRetry : public UnitTest{
public:
    Test_CheckpointRetry(CheckpointRetryCase test_case)
        : UnitTest(std::format("Pokemon::CheckpointRetry - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        AutoStoryStats stats;
        const CheckpointRetryRecord record = run_case(m_case.input, stats);
        const CheckpointRetryExpectation& expected = m_case.expectation;
        if (record.passed_on != expected.passed_on){
            return std::format("Expected a pass on attempt {}, got {}", expected.passed_on, record.passed_on);
        }
        if (record.reports != expected.reports){
            return std::format("Expected {} reports, got {}", expected.reports.size(), record.reports.size());
        }
        if (record.resets != expected.resets || stats.m_reset.load() != expected.resets){
            return std::format(
                "Expected {} resets, got {} (stats {})",
                expected.resets, record.resets, stats.m_reset.load()
            );
        }
        if (record.log != expected.log){
            return std::format("Expected {} log lines, got {}", expected.log.size(), record.log.size());
        }
        return true;
    }

private:
    const CheckpointRetryCase m_case;
};

const CheckpointRetryCase CHECKPOINT_RETRY_CASES[] = {
    {
        .input = {.name = "one-shot pass", .policy = {.max_attempts = 3, .report_at_failure = 2}, .failures = 0},
        .expectation = {
            .passed_on = 1,
            .reports = {},
            .resets = 0,
            .log = {
                "[Checkpoint] 01: start",
                "[Checkpoint] 01: attempt 1/3",
                "[Checkpoint] 01: passed on attempt 1 (one-shot)",
            },
        },
    },
    {
        .input = {.name = "pass after a reset", .policy = {.max_attempts = 3, .report_at_failure = 2}, .failures = 1},
        .expectation = {
            .passed_on = 2,
            .reports = {},
            .resets = 1,
            .log = {
                "[Checkpoint] 01: start",
                "[Checkpoint] 01: attempt 1/3",
                "[Checkpoint] 01: failed, resetting for attempt 2",
                "[Checkpoint] 01: attempt 2/3",
                "[Checkpoint] 01: passed on attempt 2 (1 reset)",
            },
        },
    },
    {
        .input = {.name = "report once at its failure count", .policy = {.max_attempts = 5, .report_at_failure = 2}, .failures = 3},
        .expectation = {
            .passed_on = 4,
            .reports = {2},
            .resets = 3,
            .log = {
                "[Checkpoint] 01: start",
                "[Checkpoint] 01: attempt 1/5",
                "[Checkpoint] 01: failed, resetting for attempt 2",
                "[Checkpoint] 01: attempt 2/5",
                "[Checkpoint] 01: failed, resetting for attempt 3",
                "[Checkpoint] 01: attempt 3/5",
                "[Checkpoint] 01: failed, resetting for attempt 4",
                "[Checkpoint] 01: attempt 4/5",
                "[Checkpoint] 01: passed on attempt 4 (3 resets)",
            },
        },
    },
    {
        .input = {.name = "give up after the last attempt, without a reset", .policy = {.max_attempts = 2, .report_at_failure = 2}, .failures = 5},
        .expectation = {
            .passed_on = 0,
            .reports = {2},
            .resets = 1,
            .log = {
                "[Checkpoint] 01: start",
                "[Checkpoint] 01: attempt 1/2",
                "[Checkpoint] 01: failed, resetting for attempt 2",
                "[Checkpoint] 01: attempt 2/2",
            },
        },
    },
};


//  A checkpoint whose attempts end as `results` says, in order, and whose
//  resets fail from `reset_fails_at` on (0: never).
struct CheckpointAttemptsInput{
    std::string name;
    size_t max_attempts;
    std::vector<AttemptResult> results;
    size_t reset_fails_at = 0;
};

struct CheckpointAttemptsExpectation{
    bool passed;
    size_t attempts;
    //  The attempt each reset was for.
    std::vector<size_t> resets;
    std::vector<std::string> log;
};

// One run of the shared loop and what it must do.
struct CheckpointAttemptsCase{
    CheckpointAttemptsInput input;
    CheckpointAttemptsExpectation expectation;
};

class Test_CheckpointAttempts : public UnitTest{
public:
    Test_CheckpointAttempts(CheckpointAttemptsCase test_case)
        : UnitTest(std::format("Pokemon::CheckpointRetry - attempts: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const CheckpointAttemptsInput& input = m_case.input;
        std::vector<size_t> resets;
        std::vector<std::string> log;
        const CheckpointAttempts attempts{
            .max_attempts = input.max_attempts,
            .run = [&](size_t attempt){
                return attempt <= input.results.size() ? input.results[attempt - 1] : AttemptResult::FINAL;
            },
            .reset = [&](size_t next_attempt){
                resets.push_back(next_attempt);
                return input.reset_fails_at == 0 || next_attempt < input.reset_fails_at;
            },
            .log = [&](const std::string& line){
                log.push_back(line);
            },
        };
        const CheckpointAttemptsResult result = run_checkpoint_attempts("01", attempts);
        const CheckpointAttemptsExpectation& expected = m_case.expectation;
        if (result.passed != expected.passed || result.attempts != expected.attempts){
            return std::format(
                "Expected passed {} after {} attempts, got {} after {}",
                expected.passed, expected.attempts, result.passed, result.attempts
            );
        }
        if (resets != expected.resets){
            return std::format("Expected {} resets, got {}", expected.resets.size(), resets.size());
        }
        if (log != expected.log){
            return std::format("Expected {} log lines, got {}", expected.log.size(), log.size());
        }
        return true;
    }

private:
    const CheckpointAttemptsCase m_case;
};

const CheckpointAttemptsCase CHECKPOINT_ATTEMPTS_CASES[] = {
    {
        .input = {.name = "pass after a reset", .max_attempts = 3, .results = {AttemptResult::FAILED, AttemptResult::PASSED}},
        .expectation = {
            .passed = true,
            .attempts = 2,
            .resets = {2},
            .log = {"[Checkpoint] 01: passed on attempt 2 (1 reset)"},
        },
    },
    {
        .input = {.name = "a final failure stops without a reset", .max_attempts = 3, .results = {AttemptResult::FINAL}},
        .expectation = {.passed = false, .attempts = 1, .resets = {}, .log = {}},
    },
    {
        .input = {.name = "no reset after the last attempt", .max_attempts = 2, .results = {AttemptResult::FAILED, AttemptResult::FAILED}},
        .expectation = {.passed = false, .attempts = 2, .resets = {2}, .log = {}},
    },
    {
        .input = {
            .name = "a failed reset stops",
            .max_attempts = 3,
            .results = {AttemptResult::FAILED, AttemptResult::FAILED},
            .reset_fails_at = 3,
        },
        .expectation = {.passed = false, .attempts = 2, .resets = {2, 3}, .log = {}},
    },
};


//  `saves` checkpoint saves in a row.
struct CheckpointSavedInput{
    std::string name;
    size_t saves;
};

struct CheckpointSavedExpectation{
    uint64_t checkpoints;
    size_t updates;
    std::vector<std::string> messages;
};

// Checkpoint saves and what they must count and send.
struct CheckpointSavedCase{
    CheckpointSavedInput input;
    CheckpointSavedExpectation expectation;
};

class Test_CheckpointSaved : public UnitTest{
public:
    Test_CheckpointSaved(CheckpointSavedCase test_case)
        : UnitTest(std::format("Pokemon::CheckpointRetry - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        AutoStoryStats stats;
        size_t updates = 0;
        std::vector<std::string> messages;
        for (size_t i = 0; i < m_case.input.saves; i++){
            record_checkpoint_saved(
                [&](){ updates++; },
                [&](std::string_view message){ messages.emplace_back(message); },
                stats
            );
        }
        const CheckpointSavedExpectation& expected = m_case.expectation;
        if (stats.m_checkpoint.load() != expected.checkpoints || updates != expected.updates){
            return std::format(
                "Expected {} checkpoints and {} updates, got {} and {}",
                expected.checkpoints, expected.updates, stats.m_checkpoint.load(), updates
            );
        }
        if (messages != expected.messages){
            return std::format("Expected {} messages, got {}", expected.messages.size(), messages.size());
        }
        return true;
    }

private:
    const CheckpointSavedCase m_case;
};

const CheckpointSavedCase CHECKPOINT_SAVED_CASES[] = {
    {
        .input = {.name = "each save counts, publishes and notifies", .saves = 2},
        .expectation = {
            .checkpoints = 2,
            .updates = 2,
            .messages = {"Saved at checkpoint.", "Saved at checkpoint."},
        },
    },
};


} // namespace


void add_tests_CheckpointRetry(UnitTestDatabase& database){
    for (const CheckpointAttemptsCase& test_case : CHECKPOINT_ATTEMPTS_CASES){
        database.add<Test_CheckpointAttempts>(test_case);
    }
    for (const CheckpointRetryCase& test_case : CHECKPOINT_RETRY_CASES){
        database.add<Test_CheckpointRetry>(test_case);
    }
    for (const CheckpointSavedCase& test_case : CHECKPOINT_SAVED_CASES){
        database.add<Test_CheckpointSaved>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
