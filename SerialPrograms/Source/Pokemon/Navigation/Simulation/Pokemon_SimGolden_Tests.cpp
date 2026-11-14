/*  Sim Golden Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The golden file plumbing on its own: paths beside a test, the text, and
 *  the first difference a check reports.
 *
 */

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon_SimGolden.h"
#include "Pokemon_SimGolden_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

struct SimGoldenTestCase{
    struct Input{
        std::string name;
        UnitTestResult (*body)(Logger& logger);
    };

    struct Expectation{
        std::string_view description;
    };

    Input input;
    Expectation expectation;
};

class Test_SimGolden : public UnitTest{
public:
    explicit Test_SimGolden(SimGoldenTestCase scenario)
        : UnitTest("Pokemon::Navigation - Sim golden: " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_scenario.input.body(logger);
    }

private:
    SimGoldenTestCase m_scenario;
};

UnitTestResult expect_equal(std::string_view what, std::string_view actual, std::string_view expected){
    if (actual == expected){
        return true;
    }
    return std::string(what) + ": expected \"" + std::string(expected) + "\", got \"" + std::string(actual) + "\".";
}

//  The difference as text for expect_equal(): "(none)" when the texts match.
std::string difference_of(std::string_view golden, std::string_view actual){
    return first_golden_difference(golden, actual).value_or("(none)");
}

UnitTestResult test_path_beside_source(Logger&){
    UnitTestResult result = expect_equal("posix", sim_golden_path("/a/b/Test.cpp", "x.golden.txt"), "/a/b/x.golden.txt");
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    result = expect_equal("windows", sim_golden_path("C:\\a\\Test.cpp", "x.txt"), "C:\\a\\x.txt");
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    return expect_equal("no directory", sim_golden_path("Test.cpp", "x.txt"), "x.txt");
}

UnitTestResult test_text(Logger&){
    return expect_equal("text", sim_golden_text("about", {"a", "b"}), "# about\na\nb\n");
}

UnitTestResult test_same_text_ignoring_crlf(Logger&){
    return expect_equal("difference", difference_of("# x\r\na\r\nb\r\n", "# x\na\nb\n"), "(none)");
}

UnitTestResult test_first_differing_line(Logger&){
    return expect_equal(
        "difference", difference_of("# x\r\na\r\nb\r\n", "# x\na\nc\n"),
        "line 3:\n--- golden: b\n+++ actual: c"
    );
}

UnitTestResult test_extra_line(Logger&){
    UnitTestResult result = expect_equal(
        "longer actual", difference_of("a\n", "a\nb\n"),
        "line 2:\n--- golden: (end of file)\n+++ actual: b"
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    return expect_equal(
        "shorter actual", difference_of("a\nb\n", "a\n"),
        "line 2:\n--- golden: b\n+++ actual: (end of file)"
    );
}

UnitTestResult test_with_crlf(Logger&){
    UnitTestResult result = expect_equal("bare LF", with_crlf("a\nb\n"), "a\r\nb\r\n");
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    result = expect_equal("CRLF", with_crlf("a\r\nb\r\n"), "a\r\nb\r\n");
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    return expect_equal("mixed and a stray CR", with_crlf("a\r\nb\rc\n"), "a\r\nbc\r\n");
}

UnitTestResult test_missing_golden_fails(Logger& logger){
    if (sim_golden_update_requested()){
        return UnitTestResult(UnitTestResult::SKIPPED, "the goldens are being rewritten");
    }
    const UnitTestResult result = check_sim_golden(logger, sim_golden_path(__FILE__, "no_such.golden.txt"), "x\n");
    if (result.result == UnitTestResult::PASSED){
        return "Expected a missing golden to fail the check.";
    }
    return true;
}

const SimGoldenTestCase SIM_GOLDEN_CASES[] = {
    {.input = {.name = "the golden's path is beside the test's source", .body = test_path_beside_source}, .expectation = {}},
    {.input = {.name = "the text is a comment line, then a line each", .body = test_text}, .expectation = {}},
    {.input = {.name = "CRLF and LF text compare the same", .body = test_same_text_ignoring_crlf}, .expectation = {}},
    {.input = {.name = "the first differing line is reported", .body = test_first_differing_line}, .expectation = {}},
    {.input = {.name = "a line more or less is reported", .body = test_extra_line}, .expectation = {}},
    {.input = {.name = "goldens are written with exactly one CRLF a line", .body = test_with_crlf}, .expectation = {}},
    {.input = {.name = "a missing golden fails", .body = test_missing_golden_fails}, .expectation = {}},
};

} // namespace


void add_tests_SimGolden(UnitTestDatabase& database){
    for (const SimGoldenTestCase& scenario : SIM_GOLDEN_CASES){
        database.add<Test_SimGolden>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
