/*  Story Run Options Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon_StoryRunOptions.h"
#include "Pokemon_StoryRunOptions_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  Two checkpoints' dropdowns.
const StringSelectDatabase& two_checkpoints(){
    static const StringSelectDatabase database = story_checkpoint_database(std::vector<std::string>{"01.01", "01.02"});
    return database;
}

//  The reload at the start a user set, and whether the options let the run
//  start.
struct ReloadAtStartCase{
    struct Input{
        bool reload_at_start;
    };
    struct Expectation{
        //  Empty: the options are valid; else the refusal's message.
        std::string_view error;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

const ReloadAtStartCase RELOAD_AT_START_CASES[] = {
    {.name = "reload at the start", .input = {.reload_at_start = true}, .expectation = {.error = ""}},
    {.name = "no reload at the start", .input = {.reload_at_start = false}, .expectation = {.error = ""}},
};

class Test_ReloadAtStart : public UnitTest{
public:
    explicit Test_ReloadAtStart(ReloadAtStartCase test_case)
        : UnitTest(std::format("Pokemon::StoryRunOptions - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        StoryRunOptions run(two_checkpoints());
        if (!run.RELOAD_AT_START){
            return "Expected the reload at the start on by default.";
        }
        run.RELOAD_AT_START = m_case.input.reload_at_start;
        const std::string error = run.check_validity();
        if (error != m_case.expectation.error){
            return std::format("Expected \"{}\", got \"{}\".", m_case.expectation.error, error);
        }
        return true;
    }

private:
    const ReloadAtStartCase m_case;
};

} // namespace


void add_tests_StoryRunOptions(UnitTestDatabase& database){
    for (const ReloadAtStartCase& test_case : RELOAD_AT_START_CASES){
        database.add<Test_ReloadAtStart>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
