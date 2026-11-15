/*  Pokemon LZA Pending Fixtures
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Image tests whose frames are not on PokemonAutomation/CommandLineTests
 *  yet. They are registered wrapped, so a checkout without the frames skips
 *  them instead of failing.
 */

#ifndef PokemonAutomation_PokemonLZA_PendingFixtures_H
#define PokemonAutomation_PokemonLZA_PendingFixtures_H

#include <format>
#include <memory>
#include <utility>
#include "Common/Cpp/Filesystem/FilePath.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "Tests/ImageUnitTest.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Runs `test` when every one of its images is in the test repo, and skips
//  it otherwise.
class PendingFixtureTest : public UnitTest{
public:
    explicit PendingFixtureTest(std::shared_ptr<const ImageUnitTest> test)
        : UnitTest(test->name())
        , m_test(std::move(test))
    {
        m_memory = m_test->memory();
    }

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        for (const std::string& path : m_test->image_paths()){
            if (!Filesystem::exists(Filesystem::Path(UNIT_TEST_RESOURCE_PATH() + path))){
                return UnitTestResult(UnitTestResult::SKIPPED, std::format("{} is not in CommandLineTests.", path));
            }
        }
        return m_test->run(logger, scope);
    }

private:
    std::shared_ptr<const ImageUnitTest> m_test;
};


//  Adds the image test `TestType(args...)`, skipped while its images are
//  missing.
template <typename TestType, class... Args>
void add_pending_fixture_test(UnitTestDatabase& database, Args&&... args){
    database += std::make_shared<PendingFixtureTest>(std::make_shared<TestType>(std::forward<Args>(args)...));
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
