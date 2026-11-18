#include <iostream>
#include <string>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

#ifndef PA_BAZEL_TEST_NS
#error "Define PA_BAZEL_TEST_NS (e.g. PokemonAutomation::NintendoSwitch::PokemonFRLG)"
#endif
#ifndef PA_BAZEL_TEST_FN
#error "Define PA_BAZEL_TEST_FN (e.g. add_tests_AutoStoryPlannedBattleFake)"
#endif

namespace PA_BAZEL_TEST_NS {
void PA_BAZEL_TEST_FN(PokemonAutomation::UnitTestDatabase& database);
}

namespace {
class StdoutLogger final : public PokemonAutomation::Logger {
public:
    void log(const std::string& msg, PokemonAutomation::Color = PokemonAutomation::Color()) override {
        std::cout << msg << '\n';
    }
};
}  // namespace

int main() {
    using namespace PokemonAutomation;
    UnitTestDatabase db;
    PA_BAZEL_TEST_NS::PA_BAZEL_TEST_FN(db);

    StdoutLogger logger;
    CancellableHolder<CancellableScope> scope;

    int total = 0;
    int failed = 0;
    for (const auto& [name, test] : db) {
        ++total;
        UnitTestResult res = test->run(logger, scope);
        if (res.result != UnitTestResult::PASSED) {
            ++failed;
            std::cerr << "[FAIL] " << name << ": " << res.message << '\n';
        }
    }
    std::cout << "Ran " << total << " tests, " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
