/*  AutoStory Select Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <memory>
#include <string>
#include <vector>
#include "Pokemon_AutoStorySelect.h"
#include "Pokemon_AutoStorySelect_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

// An entry of a story section, its index in the whole story list, and the
// index of the first main story entry.
struct AutoStoryIndexScenario{
    std::string name;
    bool main_story;
    size_t index_in_section;
    size_t first_main_story_index;
    size_t expected;
};

class Test_AutoStoryIndex : public UnitTest{
public:
    Test_AutoStoryIndex(AutoStoryIndexScenario scenario)
        : UnitTest("Pokemon::AutoStoryIndex - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const size_t got = story_index(
            m_scenario.main_story, m_scenario.index_in_section, m_scenario.first_main_story_index
        );
        if (got != m_scenario.expected){
            return "Expected index " + std::to_string(m_scenario.expected) + ", got " + std::to_string(got);
        }
        return true;
    }

private:
    const AutoStoryIndexScenario m_scenario;
};

// A stand-in for a story segment or checkpoint: only its name is read.
struct NamedEntry{
    std::string label;
    std::string name() const{ return label; }
};

class Test_AutoStorySelectDatabase : public UnitTest{
public:
    Test_AutoStorySelectDatabase()
        : UnitTest("Pokemon::AutoStorySelectDatabase - a section keeps whole-list slugs")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<std::unique_ptr<NamedEntry>> list;
        for (const char* label : {"a", "b", "c", "d"}){
            list.emplace_back(std::make_unique<NamedEntry>(NamedEntry{label}));
        }
        const StringSelectDatabase database = make_story_select_database(list, 2, 10);
        const std::vector<StringSelectEntry>& entries = database.case_list();
        if (entries.size() != 2){
            return "Expected 2 entries (the end is clamped to the list), got " + std::to_string(entries.size());
        }
        if (entries[0].slug != "2" || entries[0].display_name != "c" || entries[1].slug != "3" || entries[1].display_name != "d"){
            return "Expected slugs 2, 3 labeled c, d; got " + entries[0].slug + " " + entries[0].display_name
                + ", " + entries[1].slug + " " + entries[1].display_name;
        }
        return true;
    }
};

}

void add_tests_AutoStorySelect(UnitTestDatabase& database){
    const AutoStoryIndexScenario scenarios[] = {
        {
            .name = "a tutorial entry is its own index",
            .main_story = false,
            .index_in_section = 3,
            .first_main_story_index = 10,
            .expected = 3,
        },
        {
            // SV: segment 10 is the first of the main story.
            .name = "the first main story entry follows the tutorial",
            .main_story = true,
            .index_in_section = 0,
            .first_main_story_index = 10,
            .expected = 10,
        },
        {
            // SV: checkpoint 21 is the first of the main story.
            .name = "a later main story checkpoint",
            .main_story = true,
            .index_in_section = 5,
            .first_main_story_index = 21,
            .expected = 26,
        },
    };
    for (const AutoStoryIndexScenario& scenario : scenarios){
        database.add<Test_AutoStoryIndex>(scenario);
    }
    database.add<Test_AutoStorySelectDatabase>();
}



}
}
