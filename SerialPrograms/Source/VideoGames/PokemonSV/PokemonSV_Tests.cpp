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
#include "Programs/Farming/PokemonSV_BlueberryCatchPhoto.h"
#include "PokemonSV_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{



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

void add_tests_BBQCatchOnReturn(UnitTestDatabase& database){
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

}

void add_tests(UnitTestDatabase& database){
    add_tests_BBQCatchOnReturn(database);
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
