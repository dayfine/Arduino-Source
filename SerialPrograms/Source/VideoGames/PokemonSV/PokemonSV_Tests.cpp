/*  Pokemon SV Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
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
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Inference/PokemonSV_BlueberryPointsReader.h"
#include "Programs/Farming/PokemonSV_BBQPointsLedger.h"

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

class Test_BlueberryPoints : public UnitTest{
public:
    Test_BlueberryPoints(const std::string& image, int expected, size_t width, size_t height)
        : UnitTest("PokemonSV::BlueberryPoints - " + image + " - " + std::to_string(width))
        , m_image(UNIT_TEST_RESOURCE_PATH() + image)
        , m_expected(expected), m_width(width), m_height(height)
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        ImageRGB32 reference(m_image);
        ImageRGB32 image = reference.scale_to(m_width, m_height);
        const int actual = read_BP_from_image(logger, image);
        if (actual != m_expected){
            return "Expected " + std::to_string(m_expected) + " BP, read " + std::to_string(actual);
        }
        return true;
    }

private:
    const std::string m_image;
    const int m_expected;
    const size_t m_width, m_height;
};

// Balance reads in order, each at a minute offset from the first, and the
// ledger they must produce. A negative read is an OCR failure. "spend_before"
// is what the program paid for rerolls just before the read (-1: a reroll of
// unknown cost).
struct BBQPointsLedgerScenario{
    struct Read{
        int balance;
        int minute;
        int spend_before = 0;
    };
    std::string name;
    std::vector<Read> reads = {};
    int expected_start = -1;
    uint64_t expected_earned = 0;
    uint64_t expected_spent = 0;
    int64_t expected_net = 0;
    int64_t expected_per_hour = 0;
    uint64_t expected_checked = 0;
    uint64_t expected_mismatched = 0;
    bool expected_reconciled = false;
};

class Test_BBQPointsLedger : public UnitTest{
public:
    Test_BBQPointsLedger(BBQPointsLedgerScenario scenario)
        : UnitTest("PokemonSV::BBQPointsLedger - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const WallClock start = current_time();
        BBQPointsLedger ledger;
        for (const BBQPointsLedgerScenario::Read& read : m_scenario.reads){
            if (read.spend_before < 0){
                ledger.skip_spend_check();
            }else if (read.spend_before > 0){
                ledger.expect_spend(read.spend_before);
            }
            ledger.record(read.balance, start + std::chrono::minutes(read.minute));
        }
        const std::pair<const char*, std::pair<int64_t, int64_t>> checks[] = {
            {"starting balance", {m_scenario.expected_start, ledger.starting_balance}},
            {"earned", {(int64_t)m_scenario.expected_earned, (int64_t)ledger.earned}},
            {"spent", {(int64_t)m_scenario.expected_spent, (int64_t)ledger.spent}},
            {"net", {m_scenario.expected_net, ledger.net()}},
            {"net per hour", {m_scenario.expected_per_hour, ledger.net_per_hour()}},
            {"checked reroll windows", {(int64_t)m_scenario.expected_checked, (int64_t)ledger.checked_windows}},
            {"mismatched reroll windows", {(int64_t)m_scenario.expected_mismatched, (int64_t)ledger.mismatched_windows}},
            {"reconciled", {m_scenario.expected_reconciled, ledger.reconciled()}},
        };
        for (const auto& [field, values] : checks){
            if (values.first != values.second){
                return std::string("Expected ") + field + " " + std::to_string(values.first)
                    + ", got " + std::to_string(values.second);
            }
        }
        return true;
    }

private:
    const BBQPointsLedgerScenario m_scenario;
};

}

void add_tests_BBQPointsLedger(UnitTestDatabase& database){
    const BBQPointsLedgerScenario scenarios[] = {
        {
            .name = "no reads",
        },
        {
            .name = "one read sets the start",
            .reads = {{6750, 0}},
            .expected_start = 6750,
            .expected_reconciled = true,
        },
        {
            // A quest reward, then a reroll.
            .name = "rises are earned and drops are spent",
            .reads = {{6750, 0}, {6780, 10}, {6730, 20}},
            .expected_start = 6750,
            .expected_earned = 30,
            .expected_spent = 50,
            .expected_net = -20,
            .expected_per_hour = -60,
            .expected_reconciled = true,
        },
        {
            .name = "an unchanged balance adds nothing",
            .reads = {{6750, 0}, {6750, 30}},
            .expected_start = 6750,
            .expected_reconciled = true,
        },
        {
            .name = "OCR failures are skipped",
            .reads = {{6750, 0}, {-1, 5}, {6800, 30}},
            .expected_start = 6750,
            .expected_earned = 50,
            .expected_net = 50,
            .expected_per_hour = 100,
            .expected_reconciled = true,
        },
        {
            // The hour starts at the first successful read.
            .name = "a failed first read does not set the start",
            .reads = {{-1, 0}, {6750, 10}, {6760, 70}},
            .expected_start = 6750,
            .expected_earned = 10,
            .expected_net = 10,
            .expected_per_hour = 10,
            .expected_reconciled = true,
        },
        {
            // A blue and a red reroll, each followed by a read, as the farmer does.
            .name = "rerolls that match the reads reconcile",
            .reads = {{6750, 0}, {6740, 1, 10}, {6800, 10}, {6750, 11, 50}},
            .expected_start = 6750,
            .expected_earned = 60,
            .expected_spent = 60,
            .expected_per_hour = 0,
            .expected_checked = 2,
            .expected_reconciled = true,
        },
        {
            // The game didn't take the BP: the reroll didn't happen.
            .name = "a reroll that cost nothing is a mismatch",
            .reads = {{6750, 0}, {6750, 1, 10}},
            .expected_start = 6750,
            .expected_checked = 1,
            .expected_mismatched = 1,
        },
        {
            // 6740 misread as 6140.
            .name = "a misread after a reroll is a mismatch",
            .reads = {{6750, 0}, {6140, 1, 10}},
            .expected_start = 6750,
            .expected_spent = 610,
            .expected_net = -610,
            .expected_per_hour = -36600,
            .expected_checked = 1,
            .expected_mismatched = 1,
        },
        {
            // The next read may include quest income, so the window isn't checked.
            .name = "a failed read after a reroll skips the check",
            .reads = {{6750, 0}, {-1, 1, 10}, {6790, 10}},
            .expected_start = 6750,
            .expected_earned = 40,
            .expected_net = 40,
            .expected_per_hour = 240,
            .expected_reconciled = true,
        },
        {
            .name = "a reroll of unknown cost skips the check",
            .reads = {{6750, 0}, {6700, 1, -1}},
            .expected_start = 6750,
            .expected_spent = 50,
            .expected_net = -50,
            .expected_per_hour = -3000,
            .expected_reconciled = true,
        },
    };
    for (const BBQPointsLedgerScenario& scenario : scenarios){
        database.add<Test_BBQPointsLedger>(scenario);
    }
}

void add_tests(UnitTestDatabase& database){
    database.add<Test_BBQuestsSlugs>();

    // Full-frame references, in the CommandLineTests repo: they exercise the production
    // crop, filter and OCR together.
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1280, 720);

    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1280, 720);

    add_tests_BBQPointsLedger(database);

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
