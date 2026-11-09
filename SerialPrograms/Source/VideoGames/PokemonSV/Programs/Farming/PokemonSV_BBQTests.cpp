/*  Pokemon SV BBQ Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Tests/ImageUnitTest.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_BlueberryPointsReader.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_BBQuests.h"
#include "PokemonSV_BlueberryCatchPhoto.h"
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

// A catch quest, whether this attempt already battled, and whether a wild
// battle on the way back to Central Plaza should be caught instead of fled.
struct BBQCatchOnReturnScenario{
    std::string name;
    BBQuests quest;
    bool battled_this_attempt = false;
    bool expected_catch = false;
};

class Test_BBQCatchOnReturn : public UnitTest{
public:
    Test_BBQCatchOnReturn(BBQCatchOnReturnScenario scenario)
        : UnitTest("PokemonSV::BBQCatchOnReturn - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const bool got = catch_quest_catches_on_return(m_scenario.quest, m_scenario.battled_this_attempt);
        if (got != m_scenario.expected_catch){
            return std::string("Expected ") + (m_scenario.expected_catch ? "catch" : "flee")
                + ", got " + (got ? "catch" : "flee");
        }
        return true;
    }

private:
    const BBQCatchOnReturnScenario m_scenario;
};

}

static void add_tests_BBQCatchOnReturn(UnitTestDatabase& database){
    using enum BBQuests;
    const BBQCatchOnReturnScenario scenarios[] = {
        {
            // The target spawned late or not at all: any wild Pokemon completes the quest.
            .name = "catch any without a battle catches",
            .quest = catch_any,
            .expected_catch = true,
        },
        {
            // Already caught this attempt: fleeing is faster than a second catch.
            .name = "catch any after a battle flees",
            .quest = catch_any,
            .battled_this_attempt = true,
        },
        {
            // The type of a wild Pokemon is not read in battle.
            .name = "catch normal without a battle flees",
            .quest = catch_normal,
        },
        {
            .name = "catch fire without a battle flees",
            .quest = catch_fire,
        },
        {
            .name = "catch psychic without a battle flees",
            .quest = catch_psychic,
        },
        {
            .name = "catch fighting after a battle flees",
            .quest = catch_fighting,
            .battled_this_attempt = true,
        },
    };
    for (const BBQCatchOnReturnScenario& scenario : scenarios){
        database.add<Test_BBQCatchOnReturn>(scenario);
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
    add_tests_TeraEncounterAction(database);
    add_tests_BBQCatchOnReturn(database);
    add_tests_BBQTargetAttempt(database);
}

}
}
}
