/*  Lab Startup Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Regression context: fork PR #226, 01e665911, lab controller/permissions lesson.
 *  A typo in a test command must never restore a saved controller panel.
 */
#include <format>
#include <vector>
#include "LabStartupPolicy.h"
#include "LabStartupPolicy_Tests.h"

namespace PokemonAutomation{
namespace{

struct LabStartupInput{
    std::string_view name;
    std::vector<std::string_view> arguments;
    LabStartupPolicy policy;
};

struct LabStartupExpectation{
    StartupMode mode;
    std::optional<std::string> profile = std::nullopt;
    bool augment = false;
    int gui_calls = 0;
    int test_calls = 0;
    int exit_code = 2;
    std::vector<std::string> qt_arguments = {};
};

struct LabStartupCase{
    LabStartupInput input;
    LabStartupExpectation expectation;
};

const LabStartupCase LAB_STARTUP_CASES[] = {
    {.input = {.name = "unbound relaunch", .arguments = {}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "bound relaunch", .arguments = {}, .policy = {LabProfileMode::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::GUI, .profile = "lab-B", .gui_calls = 1, .exit_code = 17}},
    {.input = {.name = "explicit identity", .arguments = {"--profile", "lab-B"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::GUI, .profile = "lab-B", .gui_calls = 1, .exit_code = 17}},
    {.input = {.name = "matching identity", .arguments = {"--profile", "lab-B"}, .policy = {LabProfileMode::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::GUI, .profile = "lab-B", .gui_calls = 1, .exit_code = 17}},
    {.input = {.name = "wrong console", .arguments = {"--profile", "lab-A"}, .policy = {LabProfileMode::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "missing profile", .arguments = {"--profile"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "empty profile", .arguments = {"--profile", ""}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "duplicate profile", .arguments = {"--profile", "lab-B", "--profile", "lab-B"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "path traversal", .arguments = {"--profile", "../lab-A"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "absolute profile", .arguments = {"--profile", "/lab-A"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "space in profile", .arguments = {"--profile", "lab B"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "unicode profile", .arguments = {"--profile", "lab-é"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "option as profile", .arguments = {"--profile", "--command-line-test-mode"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "test typo from incident", .arguments = {"--test", "bdsp"}, .policy = {LabProfileMode::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "unknown after test flag", .arguments = {"--command-line-test-mode", "bdsp"}, .policy = {}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "unsupported filter", .arguments = {"--command-line-test-mode", "--test-filter=bdsp"}, .policy = {}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "test bypasses profile requirement", .arguments = {"--command-line-test-mode"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::TESTS, .test_calls = 1, .exit_code = 23}},
    {.input = {.name = "test bypasses bound console", .arguments = {"--command-line-test-mode"}, .policy = {LabProfileMode::REQUIRED, "lab-B"}},
     .expectation = {.mode = StartupMode::TESTS, .test_calls = 1, .exit_code = 23}},
    {.input = {.name = "augmented tests", .arguments = {"--augment-color-distortions", "--command-line-test-mode"}, .policy = {}},
     .expectation = {.mode = StartupMode::TESTS, .augment = true, .test_calls = 1, .exit_code = 23}},
    {.input = {.name = "augmentation without tests", .arguments = {"--augment-color-distortions"}, .policy = {}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "tests cannot select live profile", .arguments = {"--command-line-test-mode", "--profile", "lab-A"}, .policy = {LabProfileMode::REQUIRED}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "non mac GUI unchanged", .arguments = {}, .policy = {}},
     .expectation = {.mode = StartupMode::GUI, .gui_calls = 1, .exit_code = 17}},
    {.input = {.name = "unsupported profile platform", .arguments = {"--profile", "lab-B"}, .policy = {}},
     .expectation = {.mode = StartupMode::REJECTED}},
    {.input = {.name = "invalid build identity", .arguments = {}, .policy = {LabProfileMode::REQUIRED, "../lab-A"}},
     .expectation = {.mode = StartupMode::REJECTED}},
};

// The GUI callback represents settings load, window restoration and hardware discovery.
// A rejected or test launch must never reach it, even when a saved panel has devices.
UnitTestResult check_lab_startup_case(const LabStartupCase& test){
    const StartupPlan plan = apply_lab_startup_policy(parse_startup_arguments(test.input.arguments), test.input.policy);
    const LabStartupExpectation& expected = test.expectation;
    if (plan.mode != expected.mode || plan.profile != expected.profile ||
        plan.augment_color_distortions != expected.augment){
        return std::format("Expected mode {}, profile {}, augment {}; read mode {}, profile {}, augment {}.",
            (int)expected.mode, expected.profile.value_or("<none>"), expected.augment,
            (int)plan.mode, plan.profile.value_or("<none>"), plan.augment_color_distortions);
    }
    if (plan.qt_arguments != expected.qt_arguments){
        return "Expected Qt argument sequence differs from the preserved sequence.";
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

class LabStartupTest : public UnitTest{
public:
    explicit LabStartupTest(const LabStartupCase& test)
        : UnitTest("Startup/LabPolicy/" + std::string(test.input.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_lab_startup_case(m_test);
    }
private:
    const LabStartupCase& m_test;
};

} // namespace

void add_tests_LabStartupPolicy(UnitTestDatabase& database){
    for (const LabStartupCase& test : LAB_STARTUP_CASES){
        database.add<LabStartupTest>(test);
    }
}

} // namespace PokemonAutomation
