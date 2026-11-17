#include <iostream>
#include <string>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonFRLG {
void add_tests_AutoStoryPlannedBattleFake(UnitTestDatabase& database);

std::string_view status_name(StatusCondition status) {
    switch (status) {
    case StatusCondition::NONE:
        return "no status";
    case StatusCondition::POISON:
        return "PSN";
    case StatusCondition::PARALYSIS:
        return "PAR";
    case StatusCondition::SLEEP:
        return "SLP";
    case StatusCondition::OTHER:
        return "a status";
    }
    return "?";
}

std::string_view cure_of(StatusCondition status) {
    switch (status) {
    case StatusCondition::SLEEP:
        return "awakening";
    case StatusCondition::POISON:
        return "antidote";
    case StatusCondition::PARALYSIS:
        return "parlyz-heal";
    case StatusCondition::NONE:
    case StatusCondition::OTHER:
        return "";
    }
    return "";
}
}
}
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
    NintendoSwitch::PokemonFRLG::add_tests_AutoStoryPlannedBattleFake(db);

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
