/*  Checkpoint Retry Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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


class Test_CheckpointSaved : public UnitTest{
public:
    Test_CheckpointSaved()
        : UnitTest("Pokemon::CheckpointRetry - a save counts, publishes and notifies")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        AutoStoryStats stats;
        size_t updates = 0;
        std::vector<std::string> messages;
        for (size_t i = 0; i < 2; i++){
            record_checkpoint_saved(
                stats,
                [&](){ updates++; },
                [&](std::string_view message){ messages.emplace_back(message); }
            );
        }
        if (stats.m_checkpoint.load() != 2 || updates != 2){
            return std::format("Expected 2 checkpoints and updates, got {} and {}", stats.m_checkpoint.load(), updates);
        }
        if (messages != std::vector<std::string>{"Saved at checkpoint.", "Saved at checkpoint."}){
            return std::string("Expected \"Saved at checkpoint.\" per save");
        }
        return true;
    }
};


} // namespace


void add_tests_CheckpointRetry(UnitTestDatabase& database){
    for (const CheckpointRetryCase& test_case : CHECKPOINT_RETRY_CASES){
        database.add<Test_CheckpointRetry>(test_case);
    }
    database.add<Test_CheckpointSaved>();
}



} // namespace Pokemon
} // namespace PokemonAutomation
