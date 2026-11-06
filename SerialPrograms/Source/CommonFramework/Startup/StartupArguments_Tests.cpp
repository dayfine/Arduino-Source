/*  Application Startup Argument Tests
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <format>
#include "StartupArguments.h"
#include "StartupArguments_Tests.h"

namespace PokemonAutomation{
namespace{

struct ArgumentInput{
    std::string_view name;
    std::vector<std::string_view> arguments;
};

struct ArgumentExpectation{
    StartupMode mode;
    std::optional<std::string> profile = std::nullopt;
    std::vector<std::string> qt_arguments = {};
};

struct ArgumentCase{
    ArgumentInput input;
    ArgumentExpectation expectation;
};

const ArgumentCase ARGUMENT_CASES[] = {
    {.input = {.name = "ordinary GUI", .arguments = {}},
     .expectation = {.mode = StartupMode::GUI}},
    {.input = {.name = "optional profile", .arguments = {"--profile", "lab-B"}},
     .expectation = {.mode = StartupMode::GUI, .profile = "lab-B"}},
    {.input = {.name = "test dispatch", .arguments = {"--command-line-test-mode"}},
     .expectation = {.mode = StartupMode::TESTS}},
    {.input = {.name = "Qt platform", .arguments = {"-platform", "offscreen"}},
     .expectation = {.mode = StartupMode::GUI, .qt_arguments = {"-platform", "offscreen"}}},
    {.input = {.name = "Qt style assignment", .arguments = {"-style=Fusion"}},
     .expectation = {.mode = StartupMode::GUI, .qt_arguments = {"-style=Fusion"}}},
    {.input = {.name = "Qt style pair", .arguments = {"-style", "Fusion"}},
     .expectation = {.mode = StartupMode::GUI, .qt_arguments = {"-style", "Fusion"}}},
    {.input = {.name = "Qt stylesheet assignment", .arguments = {"-stylesheet=some path.qss"}},
     .expectation = {.mode = StartupMode::GUI, .qt_arguments = {"-stylesheet=some path.qss"}}},
    {.input = {.name = "Qt flags and application profile", .arguments = {"-reverse", "--profile", "lab-B", "-widgetcount"}},
     .expectation = {.mode = StartupMode::GUI, .profile = "lab-B", .qt_arguments = {"-reverse", "-widgetcount"}}},
    {.input = {.name = "repeated Qt plugin", .arguments = {"-plugin", "one", "-plugin", "two"}},
     .expectation = {.mode = StartupMode::GUI, .qt_arguments = {"-plugin", "one", "-plugin", "two"}}},
    {.input = {.name = "Qt missing value", .arguments = {"-platform"}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "Qt empty value", .arguments = {"-style", ""}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "Qt empty assignment", .arguments = {"-style="}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "Qt must not swallow application option", .arguments = {"-platform", "--command-line-test-mode"}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "Qt must not swallow typo", .arguments = {"-platform", "--test", "bdsp"}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "unknown Qt-looking option", .arguments = {"-platfrom", "offscreen"}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "unknown after valid Qt option", .arguments = {"-reverse", "--test", "bdsp"}},
     .expectation = {.mode = StartupMode::REJECTED}},
};

UnitTestResult check_argument_case(const ArgumentCase& test){
    const StartupPlan plan = parse_startup_arguments(test.input.arguments);
    const ArgumentExpectation& expected = test.expectation;
    if (plan.mode != expected.mode || plan.profile != expected.profile){
        return std::format("Expected mode {} and profile {}; read mode {} and profile {}.",
            (int)expected.mode, expected.profile.value_or("<none>"),
            (int)plan.mode, plan.profile.value_or("<none>"));
    }
    if (plan.qt_arguments != expected.qt_arguments){
        return "Expected the complete Qt argument sequence to be preserved in order.";
    }
    return true;
}

class ArgumentTest : public UnitTest{
public:
    explicit ArgumentTest(const ArgumentCase& test)
        : UnitTest("Startup/Arguments/" + std::string(test.input.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_argument_case(m_test);
    }
private:
    const ArgumentCase& m_test;
};

} // namespace

void add_tests_StartupArguments(UnitTestDatabase& database){
    for (const ArgumentCase& test : ARGUMENT_CASES){
        database.add<ArgumentTest>(test);
    }
}

} // namespace PokemonAutomation
