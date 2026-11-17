/*  Sim Sweep Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The sweep runner on fake rows: the result's text and log lines, the
 *  clean-first rule, and a variant's comparison with the clean run.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon_SimSweep.h"
#include "Pokemon_SimSweep_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  Keeps every line logged, for a test to compare.
class LinesLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string& msg, Color) override{
        lines.push_back(msg);
    }

    std::vector<std::string> lines;
};

//  What a test checks of a UnitTestResult and the lines logged.
struct ResultExpectation{
    bool passes = true;
    std::string message = {};
    std::vector<std::string> log = {};
};

UnitTestResult expect_result(const UnitTestResult& actual, const std::vector<std::string>& log, const ResultExpectation& expected){
    const bool passed = actual.result == UnitTestResult::PASSED;
    if (passed != expected.passes){
        return std::format("expected {}, got {}: \"{}\".", expected.passes ? "a pass" : "a failure", passed ? "a pass" : "a failure", actual.message);
    }
    if (actual.message != expected.message){
        return std::format("message: expected \"{}\", got \"{}\".", expected.message, actual.message);
    }
    if (log != expected.log){
        std::string out = "log: expected";
        for (const std::string& line : expected.log){
            out += std::format("\n  {}", line);
        }
        out += "\ngot";
        for (const std::string& line : log){
            out += std::format("\n  {}", line);
        }
        return out;
    }
    return true;
}


struct SweepResultCase{
    struct Input{
        std::string row;
        std::string fault;
        size_t variants;
        std::vector<SweepFailure> failures;
    };

    std::string name;
    Input input;
    ResultExpectation expectation;
};

const SweepResultCase SWEEP_RESULT_CASES[] = {
    {
        .name = "no failures pass with one count line",
        .input = {.row = "01.05", .fault = "scene end pose", .variants = 4, .failures = {}},
        .expectation = {.passes = true, .message = "", .log = {"Fault sweep 01.05 scene end pose: 4 variants, 0 fail."}},
    },
    {
        .name = "failures fail with a table line each",
        .input = {.row = "02.02", .fault = "lost tap", .variants = 3, .failures = {
            {.step = "below_oak", .variant = "tap 2", .stopped = "ended on Route1 (3, 4), not Route1 (3, 5)."},
            {.step = "", .variant = "seed 7", .stopped = "stopped in oak: no dialog."},
        }},
        .expectation = {
            .passes = false,
            .message = "2 of 3 variants fail 02.02:\n  below_oak tap 2: ended on Route1 (3, 4), not Route1 (3, 5).\n   seed 7: stopped in oak: no dialog.",
            .log = {
                "Fault sweep 02.02 lost tap: 3 variants, 2 fail.",
                "| 02.02 | below_oak | lost tap | tap 2 | ended on Route1 (3, 4), not Route1 (3, 5). |",
                "| 02.02 |  | lost tap | seed 7 | stopped in oak: no dialog. |",
            },
        },
    },
};

class Test_SweepResult : public UnitTest{
public:
    explicit Test_SweepResult(SweepResultCase test_case)
        : UnitTest(std::format("Pokemon::Navigation - Sim sweep result: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        LinesLogger logger;
        const SweepResultCase::Input& input = m_case.input;
        const UnitTestResult result = sweep_result(logger, input.row, input.fault, input.variants, input.failures);
        return expect_result(result, logger.lines, m_case.expectation);
    }

private:
    SweepResultCase m_case;
};


//  A variant's outcome in a fake sweep: why it failed, or nothing.
struct FakeVariant{
    std::string step;
    std::string label;
    std::optional<std::string> stopped;
};

struct RunSweepCase{
    struct Input{
        //  Why the fake row fails clean, or nothing.
        std::optional<std::string> clean_failure;
        std::vector<FakeVariant> variants;
    };
    struct Expectation{
        ResultExpectation result;
        //  How many variants ran.
        size_t ran;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

const RunSweepCase RUN_SWEEP_CASES[] = {
    {
        .name = "a row that fails clean is not swept, and passes",
        .input = {.clean_failure = "stopped in oak: no dialog.", .variants = {{.step = "a", .label = "v", .stopped = "x"}}},
        .expectation = {
            .result = {.passes = true, .message = "", .log = {"Fault sweep 03.01: not swept, the row fails clean: stopped in oak: no dialog."}},
            .ran = 0,
        },
    },
    {
        .name = "every variant runs; those that stop are the failures",
        .input = {.clean_failure = std::nullopt, .variants = {
            {.step = "a", .label = "ends on (1, 2)", .stopped = std::nullopt},
            {.step = "a", .label = "ends on (2, 2)", .stopped = "ended on (2, 2), not (1, 2)."},
            {.step = "b", .label = "ends on (5, 5)", .stopped = std::nullopt},
        }},
        .expectation = {
            .result = {
                .passes = false,
                .message = "1 of 3 variants fail 03.01:\n  a ends on (2, 2): ended on (2, 2), not (1, 2).",
                .log = {
                    "Fault sweep 03.01 scene end pose: 3 variants, 1 fail.",
                    "| 03.01 | a | scene end pose | ends on (2, 2) | ended on (2, 2), not (1, 2). |",
                },
            },
            .ran = 3,
        },
    },
};

class Test_RunSweep : public UnitTest{
public:
    explicit Test_RunSweep(RunSweepCase test_case)
        : UnitTest(std::format("Pokemon::Navigation - Sim sweep run: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        LinesLogger logger;
        size_t ran = 0;
        const UnitTestResult result = sweep_clean_first(logger, "03.01", m_case.input.clean_failure, [&]{
            std::vector<SweepVariant> variants;
            for (const FakeVariant& fake : m_case.input.variants){
                variants.push_back({.step = fake.step, .label = fake.label, .run = [&ran, stopped = fake.stopped]{
                    ran++;
                    return stopped;
                }});
            }
            return run_sweep(logger, "03.01", "scene end pose", variants);
        });
        if (ran != m_case.expectation.ran){
            return std::format("ran {} variants, expected {}.", ran, m_case.expectation.ran);
        }
        return expect_result(result, logger.lines, m_case.expectation.result);
    }

private:
    RunSweepCase m_case;
};


struct VariantFailureCase{
    struct Input{
        std::optional<std::string> failure;
        std::string end;
        std::string clean_end;
    };
    struct Expectation{
        std::optional<std::string> stopped;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

const VariantFailureCase VARIANT_FAILURE_CASES[] = {
    {
        .name = "same end, no failure: passes",
        .input = {.failure = std::nullopt, .end = "Route1 (3, 5)", .clean_end = "Route1 (3, 5)"},
        .expectation = {.stopped = std::nullopt},
    },
    {
        .name = "the variant's own failure comes first",
        .input = {.failure = "stopped in oak.", .end = "Route1 (3, 4)", .clean_end = "Route1 (3, 5)"},
        .expectation = {.stopped = "stopped in oak."},
    },
    {
        .name = "ending elsewhere fails",
        .input = {.failure = std::nullopt, .end = "Route1 (3, 4)", .clean_end = "Route1 (3, 5)"},
        .expectation = {.stopped = "ended on Route1 (3, 4), not Route1 (3, 5)."},
    },
};

class Test_VariantFailure : public UnitTest{
public:
    explicit Test_VariantFailure(VariantFailureCase test_case)
        : UnitTest(std::format("Pokemon::Navigation - Sim sweep variant vs clean: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const VariantFailureCase::Input& input = m_case.input;
        const std::optional<std::string> actual = variant_failure(input.failure, input.end, input.clean_end);
        if (actual != m_case.expectation.stopped){
            return std::format(
                "expected {}, got {}.",
                m_case.expectation.stopped.value_or("(none)"), actual.value_or("(none)")
            );
        }
        return true;
    }

private:
    VariantFailureCase m_case;
};


} // namespace


void add_tests_SimSweep(UnitTestDatabase& database){
    for (const SweepResultCase& item : SWEEP_RESULT_CASES){
        database.add<Test_SweepResult>(item);
    }
    for (const RunSweepCase& item : RUN_SWEEP_CASES){
        database.add<Test_RunSweep>(item);
    }
    for (const VariantFailureCase& item : VARIANT_FAILURE_CASES){
        database.add<Test_VariantFailure>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
