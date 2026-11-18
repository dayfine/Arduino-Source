/*  AutoStory Resources Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checks that every file in autostory_resource_paths() is in the
 *  Resources folder the program runs with.
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "PokemonFRLG_AutoStoryResources.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

struct ResourcesInput{
    std::string_view name;
    const std::vector<std::string_view>& (*paths)();
};
struct ResourcesExpectation{
    //  The paths that may be missing; none, for a list a program loads.
    size_t missing;
};
struct ResourcesCase{
    ResourcesInput input;
    ResourcesExpectation expectation;
};

class Test_AutoStoryResources : public UnitTest{
public:
    explicit Test_AutoStoryResources(ResourcesCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryResources - {}", test_case.input.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::string missing;
        size_t count = 0;
        for (std::string_view path : m_case.input.paths()){
            if (!Filesystem::exists(Filesystem::Path(RESOURCE_PATH() + std::string(path)))){
                missing += "\n    " + std::string(path);
                count++;
            }
        }
        if (count == m_case.expectation.missing){
            return true;
        }
        return std::to_string(count) + " missing under " + RESOURCE_PATH() + ":" + missing;
    }

private:
    ResourcesCase m_case;
};

const ResourcesCase RESOURCES_CASES[] = {
    {.input = {"every file AutoStory loads is in Resources", autostory_resource_paths}, .expectation = {.missing = 0}},
};

} // namespace


void add_tests_AutoStoryResources(UnitTestDatabase& database){
    for (const ResourcesCase& item : RESOURCES_CASES){
        database.add<Test_AutoStoryResources>(item);
    }
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
