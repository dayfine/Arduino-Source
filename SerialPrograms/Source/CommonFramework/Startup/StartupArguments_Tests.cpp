/*  Lab Startup Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Regression context: fork PR #226, 01e665911, lab controller/permissions lesson.
 *  A typo in a test command must never restore a saved controller panel.
 */
#include <format>
#include <vector>
#include "StartupArguments.h"
#include "StartupArguments_Tests.h"

namespace PokemonAutomation{
namespace{

struct StartupInput{
    std::string_view name;
    std::vector<std::string_view> arguments;
    StartupPolicy policy;
};

struct StartupExpectation{
    StartupMode mode;
    std::optional<std::string> profile = std::nullopt;
    bool augment = false;
    int gui_calls = 0;
    int test_calls = 0;
    int exit_code = 2;
};

struct StartupCase{
    StartupInput input;
    StartupExpectation expectation;
};

const StartupCase STARTUP_CASES[] = {
    {.input = {.name = "unbound relaunch", .arguments = {}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "bound relaunch", .arguments = {}, .policy = {StartupProfiles::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::GUI, .profile = "lab-B", .gui_calls = 1, .exit_code = 17}},
    {.input = {.name = "explicit identity", .arguments = {"--profile", "lab-B"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::GUI, .profile = "lab-B", .gui_calls = 1, .exit_code = 17}},
    {.input = {.name = "matching identity", .arguments = {"--profile", "lab-B"}, .policy = {StartupProfiles::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::GUI, .profile = "lab-B", .gui_calls = 1, .exit_code = 17}},
    {.input = {.name = "wrong console", .arguments = {"--profile", "lab-A"}, .policy = {StartupProfiles::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "missing profile", .arguments = {"--profile"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "empty profile", .arguments = {"--profile", ""}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "duplicate profile", .arguments = {"--profile", "lab-B", "--profile", "lab-B"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "path traversal", .arguments = {"--profile", "../lab-A"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "absolute profile", .arguments = {"--profile", "/lab-A"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "space in profile", .arguments = {"--profile", "lab B"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "unicode profile", .arguments = {"--profile", "lab-é"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "option as profile", .arguments = {"--profile", "--command-line-test-mode"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "test typo from incident", .arguments = {"--test", "bdsp"}, .policy = {StartupProfiles::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "unknown after test flag", .arguments = {"--command-line-test-mode", "bdsp"}, .policy = {}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "unsupported filter", .arguments = {"--command-line-test-mode", "--test-filter=bdsp"}, .policy = {}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "test bypasses profile requirement", .arguments = {"--command-line-test-mode"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::TESTS, .test_calls = 1, .exit_code = 23}},
    {.input = {.name = "test bypasses bound console", .arguments = {"--command-line-test-mode"}, .policy = {StartupProfiles::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::TESTS, .test_calls = 1, .exit_code = 23}},
    {.input = {.name = "augmented tests", .arguments = {"--augment-color-distortions", "--command-line-test-mode"}, .policy = {}},
     .expectation = {.mode = StartupMode::TESTS, .augment = true, .test_calls = 1, .exit_code = 23}},
    {.input = {.name = "augmentation without tests", .arguments = {"--augment-color-distortions"}, .policy = {}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "tests cannot select live profile", .arguments = {"--command-line-test-mode", "--profile", "lab-A"}, .policy = {StartupProfiles::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "non mac GUI unchanged", .arguments = {}, .policy = {}},
     .expectation = {.mode = StartupMode::GUI, .gui_calls = 1, .exit_code = 17}},
    {.input = {.name = "unsupported profile platform", .arguments = {"--profile", "lab-B"}, .policy = {}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "invalid build identity", .arguments = {}, .policy = {StartupProfiles::REQUIRED, "../lab-A"}},
     .expectation = {.mode = StartupMode::REJECTED}},
};

// The GUI callback represents settings load, window restoration and hardware discovery.
// A rejected or test launch must never reach it, even when a saved panel has devices.
UnitTestResult check_startup_case(const StartupCase& test){
    const StartupPlan plan = parse_startup_arguments(test.input.arguments, test.input.policy);
    const StartupExpectation& expected = test.expectation;
    if (plan.mode != expected.mode || plan.profile != expected.profile ||
        plan.augment_color_distortions != expected.augment){
        return std::format("Expected mode {}, profile {}, augment {}; read mode {}, profile {}, augment {}.",
            (int)expected.mode, expected.profile.value_or("<none>"), expected.augment,
            (int)plan.mode, plan.profile.value_or("<none>"), plan.augment_color_distortions);
    }
    if ((plan.mode == StartupMode::REJECTED) != !plan.error.empty()){
        return "Expected rejection reason exactly when startup is rejected.";
    }
    int gui_calls = 0;
    int test_calls = 0;
    const int code = dispatch_startup(plan, {
        .gui = [&](const StartupPlan&){ gui_calls++; return 17; },
        .tests = [&](const StartupPlan&){ test_calls++; return 23; },
    });
    if (gui_calls != expected.gui_calls || test_calls != expected.test_calls || code != expected.exit_code){
        return std::format("Expected GUI calls {}, test calls {}, exit {}; read GUI calls {}, test calls {}, exit {}.",
            expected.gui_calls, expected.test_calls, expected.exit_code, gui_calls, test_calls, code);
    }
    return true;
}

class StartupTest : public UnitTest{
public:
    explicit StartupTest(const StartupCase& test)
        : UnitTest("Startup/" + std::string(test.input.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_startup_case(m_test);
    }
private:
    const StartupCase& m_test;
};

} // namespace

void add_tests_StartupArguments(UnitTestDatabase& database){
    for (const StartupCase& test : STARTUP_CASES){
        database.add<StartupTest>(test);
    }
}

} // namespace PokemonAutomation
