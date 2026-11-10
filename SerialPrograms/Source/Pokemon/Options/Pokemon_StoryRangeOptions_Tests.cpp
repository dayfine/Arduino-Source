/*  Story Range Options Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_AutoStory_Tests.h"
#include "Pokemon_StoryRangeOptions.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  A stand-in for a story checkpoint: only its name is read.
struct NamedCheckpoint{
    std::string label;
    std::string name() const{
        return label;
    }
};

//  Two tutorial checkpoints, then three of the main story, as SV's list is
//  laid out.
const std::vector<std::unique_ptr<NamedCheckpoint>>& story_list(){
    static const std::vector<std::unique_ptr<NamedCheckpoint>> list = []{
        std::vector<std::unique_ptr<NamedCheckpoint>> ret;
        for (const char* label : {"tutorial 0", "tutorial 1", "main 0", "main 1", "main 2"}){
            ret.emplace_back(std::make_unique<NamedCheckpoint>(NamedCheckpoint{.label = label}));
        }
        return ret;
    }();
    return list;
}

//  The main story's dropdowns: entries 2..4 of the list, slugged 2..4.
const StringSelectDatabase& main_story_database(){
    static const StringSelectDatabase database = make_story_select_database(story_list(), 2, story_list().size());
    return database;
}

//  The start and end set by slug, as saved settings load them; empty
//  leaves the default.
struct StoryRangeInput{
    std::string name;
    std::string start_slug;
    std::string end_slug;
};

//  The indexes in the dropdown, and whether the range is valid.
struct StoryRangeExpectation{
    size_t start;
    size_t end;
    bool valid;
};

// A range chosen in the main story's dropdowns and what the group reads from it.
struct StoryRangeCase{
    StoryRangeInput input;
    StoryRangeExpectation expectation;
};

class Test_StoryRange : public UnitTest{
public:
    Test_StoryRange(StoryRangeCase test_case)
        : UnitTest("Pokemon::StoryRangeOptions - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        StoryRangeOptions range("Checkpoints", main_story_database());
        if (!m_case.input.start_slug.empty()){
            range.START_CHECKPOINT.set_by_slug(m_case.input.start_slug);
        }
        if (!m_case.input.end_slug.empty()){
            range.END_CHECKPOINT.set_by_slug(m_case.input.end_slug);
        }
        const StoryRangeExpectation& expected = m_case.expectation;
        if (range.start_index() != expected.start || range.end_index() != expected.end){
            return "Expected indexes " + std::to_string(expected.start) + " to " + std::to_string(expected.end)
                + ", got " + std::to_string(range.start_index()) + " to " + std::to_string(range.end_index());
        }
        const std::string error = range.check_validity();
        if (error.empty() != expected.valid){
            return expected.valid ? "Expected a valid range, got: " + error : "Expected the range refused.";
        }
        if (!expected.valid && error != "The start checkpoint comes after the end checkpoint."){
            return "Unexpected message: " + error;
        }
        return true;
    }

private:
    const StoryRangeCase m_case;
};

const StoryRangeCase STORY_RANGE_CASES[] = {
    {
        .input = {.name = "the whole database by default", .start_slug = "", .end_slug = ""},
        .expectation = {.start = 0, .end = 2, .valid = true},
    },
    {
        .input = {.name = "slugs are whole-list indexes, indexes are the dropdown's", .start_slug = "3", .end_slug = "4"},
        .expectation = {.start = 1, .end = 2, .valid = true},
    },
    {
        .input = {.name = "one checkpoint", .start_slug = "3", .end_slug = "3"},
        .expectation = {.start = 1, .end = 1, .valid = true},
    },
    {
        .input = {.name = "a start after the end is refused", .start_slug = "4", .end_slug = "2"},
        .expectation = {.start = 2, .end = 0, .valid = false},
    },
};

} // namespace


void add_tests_StoryRangeOptions(UnitTestDatabase& database){
    for (const StoryRangeCase& test_case : STORY_RANGE_CASES){
        database.add<Test_StoryRange>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
