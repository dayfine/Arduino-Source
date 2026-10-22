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
#include "Programs/Farming/PokemonSV_BBQTargetDiagnostics.h"
#include "PokemonSV_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

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

void add_tests_BBQTargetAttempt(UnitTestDatabase& database){
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

}



void add_tests(UnitTestDatabase& database){
    add_tests_BBQTargetAttempt(database);
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
