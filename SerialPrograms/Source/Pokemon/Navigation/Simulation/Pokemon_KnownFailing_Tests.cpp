/*  Known Failing Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The known-failing list on fake tests: one on the list passes while it
 *  fails and fails once it passes, one off the list keeps its result, an
 *  exception is a failure, and entries naming no test are stale.
 *
 */

#include <format>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon_KnownFailing.h"
#include "Pokemon_KnownFailing_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

class LinesLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string& msg, Color) override{
        lines.push_back(msg);
    }

    std::vector<std::string> lines;
};

const KnownFailing& known_failing(){
    static const KnownFailing list({
        {"listed", "the planner's goal kinds"},
        {"listed and gone", "a fix for a test since renamed"},
    });
    return list;
}

//  How the fake test's body ends.
enum class BodyOutcome{
    PASSES,
    FAILS,
    THROWS,
};

struct KnownFailingCase{
    struct Input{
        std::string test;
        BodyOutcome body;
    };
    struct Expectation{
        bool passes;
        std::string message;
        std::vector<std::string> log;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

const KnownFailingCase KNOWN_FAILING_CASES[] = {
    {
        .name = "a listed test passes while it fails",
        .input = {.test = "listed", .body = BodyOutcome::FAILS},
        .expectation = {.passes = true, .message = "", .log = {"Known failing until the planner's goal kinds: stopped at the gate."}},
    },
    {
        .name = "a listed test fails once it passes",
        .input = {.test = "listed", .body = BodyOutcome::PASSES},
        .expectation = {
            .passes = false,
            .message = "on KNOWN_FAILING (the planner's goal kinds) but passes now: take it off the list.",
            .log = {},
        },
    },
    {
        .name = "a listed test's exception is its known failure",
        .input = {.test = "listed", .body = BodyOutcome::THROWS},
        .expectation = {.passes = true, .message = "", .log = {"Known failing until the planner's goal kinds: ParseException: no row 99.99"}},
    },
    {
        .name = "an unlisted test keeps its failure",
        .input = {.test = "unlisted", .body = BodyOutcome::FAILS},
        .expectation = {.passes = false, .message = "stopped at the gate.", .log = {}},
    },
    {
        .name = "an unlisted test keeps its pass",
        .input = {.test = "unlisted", .body = BodyOutcome::PASSES},
        .expectation = {.passes = true, .message = "", .log = {}},
    },
    {
        .name = "an unlisted test's exception is its failure",
        .input = {.test = "unlisted", .body = BodyOutcome::THROWS},
        .expectation = {.passes = false, .message = "ParseException: no row 99.99", .log = {}},
    },
};

UnitTestResult fake_body(BodyOutcome outcome){
    switch (outcome){
    case BodyOutcome::PASSES:
        return true;
    case BodyOutcome::FAILS:
        return "stopped at the gate.";
    case BodyOutcome::THROWS:
        throw ParseException("no row 99.99");
    }
    return true;
}

class Test_KnownFailing : public UnitTest{
public:
    explicit Test_KnownFailing(KnownFailingCase test_case)
        : UnitTest(std::format("Pokemon::Navigation - Known failing: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        LinesLogger logger;
        const UnitTestResult result = known_failing().run(logger, m_case.input.test, [this]{
            return fake_body(m_case.input.body);
        });
        const KnownFailingCase::Expectation& expected = m_case.expectation;
        const bool passed = result.result == UnitTestResult::PASSED;
        if (passed != expected.passes){
            return std::format("expected {}, got \"{}\".", expected.passes ? "a pass" : "a failure", result.message);
        }
        if (result.message != expected.message){
            return std::format("message: expected \"{}\", got \"{}\".", expected.message, result.message);
        }
        if (logger.lines != expected.log){
            return std::format("log: expected {} lines, got {}: \"{}\".", expected.log.size(), logger.lines.size(), logger.lines.empty() ? "" : logger.lines[0]);
        }
        return true;
    }

private:
    KnownFailingCase m_case;
};


struct StaleCase{
    struct Input{
        std::set<std::string> tests;
    };
    struct Expectation{
        std::vector<std::string> stale;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

const StaleCase STALE_CASES[] = {
    {
        .name = "an entry naming no test is stale",
        .input = {.tests = {"listed", "unlisted"}},
        .expectation = {.stale = {"listed and gone"}},
    },
    {
        .name = "entries naming tests are not",
        .input = {.tests = {"listed", "listed and gone"}},
        .expectation = {.stale = {}},
    },
};

class Test_KnownFailingStale : public UnitTest{
public:
    explicit Test_KnownFailingStale(StaleCase test_case)
        : UnitTest(std::format("Pokemon::Navigation - Known failing: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::string> stale = known_failing().stale(m_case.input.tests);
        if (stale != m_case.expectation.stale){
            return std::format("expected {} stale entries, got {}.", m_case.expectation.stale.size(), stale.size());
        }
        return true;
    }

private:
    StaleCase m_case;
};

} // namespace


void add_tests_KnownFailing(UnitTestDatabase& database){
    for (const KnownFailingCase& item : KNOWN_FAILING_CASES){
        database.add<Test_KnownFailing>(item);
    }
    for (const StaleCase& item : STALE_CASES){
        database.add<Test_KnownFailingStale>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
