/*  Pokemon SV BBQ Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_BlueberryPointsReader.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_BBQuests.h"
#include "PokemonSV_BBQTargetDiagnostics.h"
#include "PokemonSV_BlueberryQuests.h"
#include "PokemonSV_BBQTests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


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

static void add_tests_TeraEncounterAction(UnitTestDatabase& database){
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

struct BBQTargetAttemptScenario{
    std::string name;
    BBQuests quest = BBQuests::catch_any;
    TargetAttemptSignals signals;
    TargetAttemptOutcome expected_outcome = TargetAttemptOutcome::no_battle;
    bool expected_needs_history = false;
    std::string expected_summary;
};

class Test_BBQTargetAttempt : public UnitTest{
public:
    Test_BBQTargetAttempt(BBQTargetAttemptScenario scenario)
        : UnitTest("PokemonSV::BBQTargetAttempt - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TargetAttemptOutcome outcome = classify_target_attempt(m_scenario.signals);
        if (outcome != m_scenario.expected_outcome){
            return std::string("Expected outcome ") + target_attempt_outcome_to_string(m_scenario.expected_outcome)
                + ", got " + target_attempt_outcome_to_string(outcome);
        }
        const bool needs_history = target_attempt_needs_history(outcome);
        if (needs_history != m_scenario.expected_needs_history){
            return std::string("Expected needs_history=") + (m_scenario.expected_needs_history ? "true" : "false")
                + ", got " + (needs_history ? "true" : "false");
        }
        const std::string summary = target_attempt_summary(
            m_scenario.quest, m_scenario.signals, std::chrono::milliseconds(41300)
        );
        if (summary != m_scenario.expected_summary){
            return "Expected summary: " + m_scenario.expected_summary + "\nActual summary:   " + summary;
        }
        return true;
    }

private:
    const BBQTargetAttemptScenario m_scenario;
};

}

static void add_tests_BBQTargetAttempt(UnitTestDatabase& database){
    const BBQTargetAttemptScenario scenarios[] = {
        {
            .name = "catch enters battle",
            .signals = {
                .kind = TargetAttemptKind::battle,
                .encounter_detected = true,
            },
            .expected_outcome = TargetAttemptOutcome::battle_entered,
            .expected_needs_history = false,
            .expected_summary = "BBQ event=target-attempt quest=catch-any outcome=battle-entered encounter=1 battle_menu=0 elapsed=41.3s",
        },
        {
            // The failure behind "Did not enter battle. Did target spawn?".
            .name = "catch finds no battle",
            .signals = {
                .kind = TargetAttemptKind::battle,
            },
            .expected_outcome = TargetAttemptOutcome::no_battle,
            .expected_needs_history = true,
            .expected_summary = "BBQ event=target-attempt quest=catch-any outcome=no-battle encounter=0 battle_menu=0 elapsed=41.3s",
        },
        {
            .name = "sneak up finds no battle",
            .quest = BBQuests::sneak_up,
            .signals = {
                .kind = TargetAttemptKind::battle,
            },
            .expected_outcome = TargetAttemptOutcome::no_battle,
            .expected_needs_history = true,
            .expected_summary = "BBQ event=target-attempt quest=sneak-up outcome=no-battle encounter=0 battle_menu=0 elapsed=41.3s",
        },
        {
            // The route only acts on the encounter watcher, so it treats this
            // as no battle: a detector disagreement, not a missing target.
            .name = "battle menu without the encounter watcher",
            .quest = BBQuests::catch_normal,
            .signals = {
                .kind = TargetAttemptKind::battle,
                .battle_menu_seen = true,
            },
            .expected_outcome = TargetAttemptOutcome::battle_menu_without_encounter,
            .expected_needs_history = true,
            .expected_summary = "BBQ event=target-attempt quest=catch-normal outcome=battle-menu-without-encounter encounter=0 battle_menu=1 elapsed=41.3s",
        },
        {
            .name = "photo prompt seen",
            .quest = BBQuests::photo_savanna,
            .signals = {
                .kind = TargetAttemptKind::photo,
                .photo_prompt_seen = true,
            },
            .expected_outcome = TargetAttemptOutcome::photo_prompt_seen,
            .expected_needs_history = false,
            .expected_summary = "BBQ event=target-attempt quest=photo-savanna outcome=photo-prompt-seen encounter=0 photo_prompt=1 elapsed=41.3s",
        },
        {
            .name = "photo prompt timeout",
            .quest = BBQuests::photo_savanna,
            .signals = {
                .kind = TargetAttemptKind::photo,
            },
            .expected_outcome = TargetAttemptOutcome::photo_prompt_timeout,
            .expected_needs_history = true,
            .expected_summary = "BBQ event=target-attempt quest=photo-savanna outcome=photo-prompt-timeout encounter=0 photo_prompt=0 elapsed=41.3s",
        },
        {
            // A wild battle cuts the photo route short; the route retries.
            .name = "battle during photo",
            .quest = BBQuests::photo_coastal,
            .signals = {
                .kind = TargetAttemptKind::photo,
                .encounter_detected = true,
            },
            .expected_outcome = TargetAttemptOutcome::battle_during_photo,
            .expected_needs_history = true,
            .expected_summary = "BBQ event=target-attempt quest=photo-coastal outcome=battle-during-photo encounter=1 photo_prompt=0 elapsed=41.3s",
        },
    };
    for (const BBQTargetAttemptScenario& scenario : scenarios){
        database.add<Test_BBQTargetAttempt>(scenario);
    }
}


void add_tests_BBQ(UnitTestDatabase& database){
    database.add<Test_BBQuestsSlugs>();

    // Full-frame references, in the CommandLineTests repo: they exercise the production
    // crop, filter and OCR together.
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1280, 720);

    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1280, 720);

    add_tests_TeraEncounterAction(database);
    add_tests_BBQTargetAttempt(database);
}



}
}
}
