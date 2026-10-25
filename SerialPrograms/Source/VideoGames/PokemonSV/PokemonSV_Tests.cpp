/*  Pokemon SV Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Inference/PokemonSV_WhiteButtonDetector.h"
#include "Inference/Boxes/PokemonSV_BoxDetection.h"
#include "Inference/Boxes/PokemonSV_BoxEggDetector.h"
#include "Inference/Map/PokemonSV_MapDetector.h"
#include "Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "Inference/Picnics/PokemonSV_PicnicDetector.h"
#include "Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "Inference/Tera/PokemonSV_TeraCardDetector.h"
#include "Inference/Tera/PokemonSV_TeraSilhouetteReader.h"
#include "Inference/Tera/PokemonSV_TeraTypeReader.h"
#include "Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "Inference/Map/PokemonSV_FastTravelDetector.h"
#include "Inference/Map/PokemonSV_MapMenuDetector.h"
#include "Inference/Map/PokemonSV_MapPokeCenterIconDetector.h"
#include "Inference/PokemonSV_ESPEmotionDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichIngredientDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichPlateDetector.h"
#include "Inference/Overworld/PokemonSV_LetsGoKillDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichRecipeDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichHandDetector.h"
#include "PokemonSV_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{



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
        const size_t got = Pokemon::story_index(
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
        const StringSelectDatabase database = Pokemon::make_story_select_database(list, 2, 10);
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

void add_tests_AutoStory(UnitTestDatabase& database){
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

void add_tests(UnitTestDatabase& database){
    add_tests_AutoStory(database);
    add_tests_WhiteButtonDetector(database);
    add_tests_BoxInfoDetector(database);
    add_tests_BoxEggDetector(database);
    add_tests_DialogDetector(database);
    add_tests_ESPEmotionDetector(database);
    add_tests_FastTravelDetector(database);
    add_tests_MapDetector(database);
    add_tests_MapMenuDetector(database);
    add_tests_MapPokeCenterIconDetector(database);
    add_tests_OverworldDetector(database);
    add_tests_PicnicDetector(database);
    add_tests_SandwichIngredientDetector(database);
    add_tests_SandwichPlateDetector(database);
    add_tests_SandwichRecipeDetector(database);
    add_tests_SandwichHandDetector(database);
    add_tests_TeraCardDetector(database);
    add_tests_TeraSilhouetteReader(database);
    add_tests_TeraTypeReader(database);
    add_tests_NormalBattleMenus(database);
    add_tests_LetsGoKillDetector(database);
}



}
}
}
