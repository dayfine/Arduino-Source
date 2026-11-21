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
#include "Pokemon_GameLanguages.h"
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

//  The reload at the start and the save mode a user set, and whether the
//  options let the run start.
struct ReloadAtStartCase{
    struct Input{
        bool reload_at_start;
        StorySaveMode save_mode;
    };
    struct Expectation{
        //  Empty: the options are valid; else the refusal's message.
        std::string_view error;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

constexpr std::string_view NO_RELOAD_SAVES =
    "Reload the Save at the Start is off: the run starts from wherever the game is, which nothing checked, "
    "so it must not save. Set Save to None or turn the reload on.";

const ReloadAtStartCase RELOAD_AT_START_CASES[] = {
    {.name = "reload at the start by default", .input = {.reload_at_start = true, .save_mode = StorySaveMode::NONE},
     .expectation = {.error = ""}},
    {.name = "a reloaded run may save", .input = {.reload_at_start = true, .save_mode = StorySaveMode::PER_CHECKPOINT},
     .expectation = {.error = ""}},
    {.name = "no reload without saves", .input = {.reload_at_start = false, .save_mode = StorySaveMode::NONE},
     .expectation = {.error = ""}},
    {.name = "no reload refuses saves after verification", .input = {.reload_at_start = false, .save_mode = StorySaveMode::AFTER_VERIFICATION},
     .expectation = {.error = NO_RELOAD_SAVES}},
    {.name = "no reload refuses saves per checkpoint", .input = {.reload_at_start = false, .save_mode = StorySaveMode::PER_CHECKPOINT},
     .expectation = {.error = NO_RELOAD_SAVES}},
};

class Test_ReloadAtStart : public UnitTest{
public:
    explicit Test_ReloadAtStart(ReloadAtStartCase test_case)
        : UnitTest(std::format("Pokemon::StoryRunOptions - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        StoryRunOptions run(two_checkpoints(), Gen8ToGen9Languages_Database());
        if (!run.RELOAD_AT_START){
            return "Expected the reload at the start on by default.";
        }
        run.RELOAD_AT_START = m_case.input.reload_at_start;
        run.SAVE_MODE.set(m_case.input.save_mode);
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
