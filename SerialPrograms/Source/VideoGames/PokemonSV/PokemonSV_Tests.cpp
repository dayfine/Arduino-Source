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
#include "Options/PokemonSV_BBQuests.h"
#include "Programs/Farming/PokemonSV_BlueberryQuests.h"
#include "PokemonSV_Tests.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Tests/ImageUnitTest.h"
#include "Inference/PokemonSV_BlueberryPointsReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

namespace{

struct TeraEncounterScenario{
    std::string name;
    bool shiny = false;
    int encounter_watcher_result;       //  0: battle menu, 1: audio, -1: route finished first
    bool battle_menu_detected = false;  //  The route's own wait saw the battle menu.
    TeraEncounterAction expected;
};

class Test_TeraEncounterAction : public UnitTest{
public:
    Test_TeraEncounterAction(TeraEncounterScenario scenario)
        : UnitTest("PokemonSV::TeraEncounterAction - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TeraEncounterAction actual = tera_encounter_action(
            m_scenario.shiny, m_scenario.encounter_watcher_result, m_scenario.battle_menu_detected
        );
        if (actual != m_scenario.expected){
            return "Expected action " + std::to_string((int)m_scenario.expected)
                + ", got " + std::to_string((int)actual) + ".";
        }
        return true;
    }

private:
    const TeraEncounterScenario m_scenario;
};

}

void add_tests_TeraEncounterAction(UnitTestDatabase& database){
    using enum TeraEncounterAction;
    const std::vector<TeraEncounterScenario> scenarios{
        {.name = "timeout recovers without battle", .encounter_watcher_result = -1, .expected = Recover},
        {.name = "audio alone is not entry", .encounter_watcher_result = 1, .expected = Recover},
        {.name = "unknown callback is not entry", .encounter_watcher_result = 2, .expected = Recover},
        {.name = "encounter watcher menu battles", .encounter_watcher_result = 0, .expected = Battle},
        {.name = "route wait menu battles", .encounter_watcher_result = -1, .battle_menu_detected = true, .expected = Battle},
        {.name = "audio then route wait menu battles", .encounter_watcher_result = 1, .battle_menu_detected = true, .expected = Battle},
        {.name = "both menus battle", .encounter_watcher_result = 0, .battle_menu_detected = true, .expected = Battle},
        {.name = "shiny stops without entry", .shiny = true, .encounter_watcher_result = -1, .expected = StopForShiny},
        {.name = "shiny stops before battle", .shiny = true, .encounter_watcher_result = 0, .battle_menu_detected = true, .expected = StopForShiny},
    };
    for (const TeraEncounterScenario& scenario : scenarios){
        database.add<Test_TeraEncounterAction>(scenario);
    }
}

namespace{

class Test_BBQuestsSlugs : public UnitTest{
public:
    Test_BBQuestsSlugs()
        : UnitTest("PokemonSV::BBQuests - slug round trip")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        for (int c = 0; c < (int)BBQuests::UnableToDetect; c++){
            const BBQuests quest = (BBQuests)c;
            const std::string& slug = BBQuests_enum_to_string(quest);
            if (slug.empty()){
                return "Quest " + std::to_string(c) + " has no slug.";
            }
            if (BBQuests_string_to_enum(slug) != quest){
                return "Slug " + slug + " does not map back to quest " + std::to_string(c) + ".";
            }
        }
        if (!BBQuests_enum_to_string(BBQuests::UnableToDetect).empty()){
            return "UnableToDetect must map to the empty slug.";
        }
        if (BBQuests_string_to_enum("not-a-quest") != BBQuests::UnableToDetect){
            return "An unrecognized slug must map to UnableToDetect.";
        }
        return true;
    }
};

}

namespace{

class Test_BlueberryPoints : public ImageUnitTest{
public:
    Test_BlueberryPoints(const std::string& image, int expected, size_t width, size_t height)
        : ImageUnitTest("PokemonSV::BlueberryPoints - " + image + " - " + std::to_string(width), {image})
        , m_expected(expected), m_width(width), m_height(height)
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope&,
        const std::vector<ImageRGB32>& images
    ) const override{
        const ImageRGB32& reference = images[0];
        ImageRGB32 image = reference.scale_to(m_width, m_height);
        const int actual = read_BP_from_image(logger, image);
        if (actual != m_expected){
            return "Expected " + std::to_string(m_expected) + " BP, read " + std::to_string(actual);
        }
        return true;
    }

private:
    const int m_expected;
    const size_t m_width, m_height;
};

}

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
    database.add<Test_BBQuestsSlugs>();

    // Full-frame references, in the CommandLineTests repo: they exercise the production
    // crop, filter and OCR together.
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1280, 720);

    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1280, 720);

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
    add_tests_TeraEncounterAction(database);
}



}
}
}
