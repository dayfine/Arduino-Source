/*  AutoStory Resources Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "Pokemon_AutoStoryResources.h"
#include "Pokemon_AutoStoryResources_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

struct MissingResourcesCase{
    struct Input{
        std::vector<std::string_view> paths = {};
    };
    struct Expectation{
        std::vector<std::string> missing = {};
    };

    std::string_view name = {};
    Input input = {};
    Expectation expectation = {};
};

const MissingResourcesCase MISSING_RESOURCES_CASES[] = {
    {
        .name = "empty list reports nothing missing",
        .input = {.paths = {}},
        .expectation = {.missing = {}},
    },
    {
        .name = "missing relative paths are returned in order",
        .input = {.paths = {"__nonexistent_autostory_a__.png", "__nonexistent_autostory_b__.json"}},
        .expectation = {.missing = {"__nonexistent_autostory_a__.png", "__nonexistent_autostory_b__.json"}},
    },
};

class Test_MissingAutoStoryResources : public UnitTest{
public:
    explicit Test_MissingAutoStoryResources(const MissingResourcesCase& scenario)
        : UnitTest(std::format("Pokemon::AutoStoryResources - {}", scenario.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::string> got = missing_autostory_resources(
            m_scenario.input.paths, Filesystem::Path(RESOURCE_PATH())
        );
        if (got != m_scenario.expectation.missing){
            return std::format(
                "missing count mismatch: got {}, expected {}",
                got.size(), m_scenario.expectation.missing.size()
            );
        }
        return true;
    }

private:
    MissingResourcesCase m_scenario;
};

} // namespace


void add_tests_AutoStoryResources(UnitTestDatabase& database){
    for (const MissingResourcesCase& scenario : MISSING_RESOURCES_CASES){
        database.add<Test_MissingAutoStoryResources>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
