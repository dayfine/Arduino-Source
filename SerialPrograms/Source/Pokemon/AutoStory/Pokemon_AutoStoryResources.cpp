/*  AutoStory Resources
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "Pokemon_AutoStoryResources.h"

namespace PokemonAutomation{
namespace Pokemon{


std::vector<std::string> missing_autostory_resources(
    std::span<const std::string_view> paths,
    const Filesystem::Path& root
){
    std::vector<std::string> missing;
    for (std::string_view path : paths){
        if (!Filesystem::exists(root / Filesystem::Path(std::string(path)))){
            missing.emplace_back(path);
        }
    }
    return missing;
}


namespace{

class Test_AutoStoryResourceList : public UnitTest{
public:
    Test_AutoStoryResourceList(
        std::string_view suite_name,
        const std::vector<std::string_view>& (*paths)()
    )
        : UnitTest(std::format("{} - every file AutoStory loads is in Resources", suite_name))
        , m_paths(paths)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::string> missing = missing_autostory_resources(
            m_paths(), Filesystem::Path(RESOURCE_PATH())
        );
        if (missing.empty()){
            return true;
        }
        std::string list;
        for (const std::string& path : missing){
            list += "\n    " + path;
        }
        return std::format("{} missing under {}:{}", missing.size(), RESOURCE_PATH(), list);
    }

private:
    const std::vector<std::string_view>& (*m_paths)();
};

} // namespace


void add_tests_AutoStoryResourceList(
    UnitTestDatabase& database,
    std::string_view suite_name,
    const std::vector<std::string_view>& (*paths)()
){
    database.add<Test_AutoStoryResourceList>(suite_name, paths);
}



} // namespace Pokemon
} // namespace PokemonAutomation
