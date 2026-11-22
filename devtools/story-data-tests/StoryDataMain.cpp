/*  Story Data Tests Main
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Standalone runner for the Qt-free story checkpoint JSON, grid story
 *  actions/args, and story engine unit tests.
 *
 */

#include <iostream>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson_Tests.h"
#include "Pokemon/AutoStory/Grid/Pokemon_GridStoryActions_Tests.h"

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
    PokemonAutomation::Pokemon::add_tests_StoryCheckpointJson(database);
    PokemonAutomation::Pokemon::add_tests_GridStoryActions(database);
    PokemonAutomation::TestLogger logger;
    PokemonAutomation::CancellableHolder<PokemonAutomation::CancellableScope> scope;
    int failed = 0;
    size_t passed_count = 0;
    for (const auto& [name, test] : database){
        const auto result = test->run(logger, scope);
        const bool passed = result.result == PokemonAutomation::UnitTestResult::PASSED;
        std::cout << name << ": " << (passed ? "PASS" : result.message) << std::endl;
        passed_count += passed;
        failed += !passed;
    }
    std::cout << passed_count << " of " << (passed_count + failed) << " story data tests passed." << std::endl;
    return failed != 0;
}
