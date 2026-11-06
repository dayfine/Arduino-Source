/*  BBQ Diagnostic Logger Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonSV_BBQLogger.h"
#include "PokemonSV_BBQLogger_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

// Captures the production Logger interface without image or controller I/O.
class BBQTestLogger : public Logger{
public:
    virtual void log(const std::string& message, Color color) override{
        messages.push_back(message);
        colors.push_back(color);
    }
    std::vector<std::string> messages;
    std::vector<Color> colors;
};

// One board snapshot and its detection outcome.
struct BoardLogInput{
    bool panel_detected;
    std::vector<BBQuests> quests;
};

// One timed attempt, including optional timeout and retry-exhaustion events.
struct AttemptLogInput{
    BBQuests quest;
    unsigned attempt;
    unsigned limit;
    std::chrono::milliseconds route_duration;
    std::chrono::milliseconds verification_duration;
    bool completed;
    bool battle_timeout = false;
    bool retry_exhausted = false;
};

// A row exercises one board read or one complete attempt event sequence.
struct DiagnosticLogInput{
    std::string name;
    std::variant<BoardLogInput, AttemptLogInput> event;
};

// Exact message order is part of the diagnostics contract.
struct DiagnosticLogExpectation{
    std::vector<std::string> messages;
    Color final_color;
};

// Each case keeps all inputs and expected output visible together.
struct DiagnosticLogCase{
    DiagnosticLogInput input;
    DiagnosticLogExpectation expectation;
};

// Exercise the real formatter with a scripted board observation.
void emit_logs(Logger& logger, const BoardLogInput& input){
    BBQLogger(logger).board(input.panel_detected, input.quests);
}

// Fixed monotonic timestamps make durations and event order deterministic.
void emit_logs(Logger& logger, const AttemptLogInput& input){
    const BBQAttemptLog::Clock::time_point start{};
    if (input.battle_timeout){
        BBQLogger(logger).battle_entry_timeout(input.quest);
    }
    BBQAttemptLog attempt(logger, input.quest, input.attempt, input.limit, start);
    attempt.route_return(start + input.route_duration);
    attempt.result(input.completed, start + input.route_duration + input.verification_duration);
    if (input.retry_exhausted){
        BBQLogger(logger).retry_exhausted(input.quest);
    }
}

// Report the first missing, unexpected or mismatched message with its index.
class Test_BBQDiagnosticLogs : public UnitTest{
public:
    explicit Test_BBQDiagnosticLogs(DiagnosticLogCase test_case)
        : UnitTest("PokemonSV::BBQDiagnosticLogs - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        BBQTestLogger logger;
        std::visit([&](const auto& input){ emit_logs(logger, input); }, m_case.input.event);
        const auto& expected = m_case.expectation.messages;
        if (logger.messages.size() != expected.size()){
            return "Expected " + std::to_string(expected.size()) + " log messages; read "
                + std::to_string(logger.messages.size()) + ".";
        }
        for (size_t index = 0; index < expected.size(); index++){
            if (logger.messages[index] != expected[index]){
                return "Message " + std::to_string(index) + ": expected [" + expected[index]
                    + "]; read [" + logger.messages[index] + "].";
            }
        }
        if (logger.colors.back() != m_case.expectation.final_color){
            return "Final message color differs from the expected severity.";
        }
        return true;
    }

private:
    const DiagnosticLogCase m_case;
};

// Covers board order/unknown/empty output and success, exhaustion and long durations.
const DiagnosticLogCase DIAGNOSTIC_LOG_CASES[] = {
    {
        .input = {.name = "board-order", .event = BoardLogInput{
            .panel_detected = true,
            .quests = {BBQuests::photo_swim, BBQuests::sneak_up, BBQuests::auto_10},
        }},
        .expectation = {.messages = {
            "BBQ event=board panel_detected=1 quests=photo-swim,sneak-up,auto-10",
        }},
    },
    {
        .input = {.name = "undetected-board", .event = BoardLogInput{
            .panel_detected = false, .quests = {BBQuests::UnableToDetect},
        }},
        .expectation = {.messages = {"BBQ event=board panel_detected=0 quests=unknown"}},
    },
    {
        .input = {.name = "empty-board", .event = BoardLogInput{
            .panel_detected = true, .quests = {},
        }},
        .expectation = {.messages = {"BBQ event=board panel_detected=1 quests="}},
    },
    {
        .input = {.name = "success", .event = AttemptLogInput{
            .quest = BBQuests::make_tm, .attempt = 1, .limit = 2,
            .route_duration = std::chrono::milliseconds(2000),
            .verification_duration = std::chrono::milliseconds(500), .completed = true,
        }},
        .expectation = {.messages = {
            "BBQ quest=make-tm attempt=1 limit=2 event=start",
            "BBQ quest=make-tm attempt=1 limit=2 event=route_return elapsed_ms=2000 elapsed=\"2.000 seconds\"",
            "BBQ quest=make-tm attempt=1 limit=2 event=result outcome=absent_from_board verification_ms=500 verification=\"0.500 seconds\" total_ms=2500 total=\"2.500 seconds\"",
        }},
    },
    {
        .input = {.name = "retry-exhausted", .event = AttemptLogInput{
            .quest = BBQuests::catch_any, .attempt = 2, .limit = 2,
            .route_duration = std::chrono::milliseconds(3345),
            .verification_duration = std::chrono::milliseconds(765), .completed = false,
            .battle_timeout = true, .retry_exhausted = true,
        }},
        .expectation = {.messages = {
            "BBQ quest=catch-any event=battle_entry outcome=timeout",
            "BBQ quest=catch-any attempt=2 limit=2 event=start",
            "BBQ quest=catch-any attempt=2 limit=2 event=route_return elapsed_ms=3345 elapsed=\"3.345 seconds\"",
            "BBQ quest=catch-any attempt=2 limit=2 event=result outcome=still_on_board verification_ms=765 verification=\"0.765 seconds\" total_ms=4110 total=\"4.110 seconds\"",
            "BBQ quest=catch-any event=retry_exhausted recovery=skip_current_batch",
        }, .final_color = COLOR_RED},
    },
    {
        .input = {.name = "long-duration", .event = AttemptLogInput{
            .quest = BBQuests::UnableToDetect, .attempt = 256, .limit = 256,
            .route_duration = std::chrono::milliseconds(120000),
            .verification_duration = std::chrono::milliseconds(0), .completed = false,
        }},
        .expectation = {.messages = {
            "BBQ quest=unknown attempt=256 limit=256 event=start",
            "BBQ quest=unknown attempt=256 limit=256 event=route_return elapsed_ms=120000 elapsed=\"2.000 minutes\"",
            "BBQ quest=unknown attempt=256 limit=256 event=result outcome=still_on_board verification_ms=0 verification=\"0.000 seconds\" total_ms=120000 total=\"2.000 minutes\"",
        }},
    },
};

} // namespace

void add_tests_BBQDiagnosticLogs(UnitTestDatabase& database){
    for (const DiagnosticLogCase& test_case : DIAGNOSTIC_LOG_CASES){
        database.add<Test_BBQDiagnosticLogs>(test_case);
    }
}

} // namespace PokemonSV
} // namespace NintendoSwitch
} // namespace PokemonAutomation
