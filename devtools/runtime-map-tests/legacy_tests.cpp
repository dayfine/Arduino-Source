/*  Existing Native Map Decoder Regression Runner
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <iostream>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_MapData_Tests.h"

namespace PokemonAutomation{
namespace{

class TestLogger final : public Logger{
public:
    virtual void log(const std::string& message, Color) override{
        std::cerr << message << std::endl;
    }
};

} // namespace
} // namespace PokemonAutomation

int main(){
    PokemonAutomation::UnitTestDatabase database;
    PokemonAutomation::Pokemon::add_tests_MapData(database);
    PokemonAutomation::TestLogger logger;
    PokemonAutomation::CancellableHolder<PokemonAutomation::CancellableScope> scope;
    int failed = 0;
    for (const auto& [name, test] : database){
        const auto result = test->run(logger, scope);
        const bool passed = result.result == PokemonAutomation::UnitTestResult::PASSED;
        std::cout << name << ": " << (passed ? "PASS" : result.message) << std::endl;
        failed += !passed;
    }
    return failed;
}
