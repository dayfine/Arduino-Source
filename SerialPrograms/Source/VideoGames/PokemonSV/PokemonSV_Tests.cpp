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
#include "PokemonSV_Tests.h"
#include "PokemonSV_IsolatedHunt_Tests.h"
#include "PokemonSV_IsolatedHuntFlight_Tests.h"
#include "PokemonSV_IsolatedHuntPower_Tests.h"
#include "PokemonSV_SandwichEating_Tests.h"
#include "PokemonSV_IsolatedHuntLoop_Tests.h"
#include "PokemonSV_IsolatedHuntCorridor_Tests.h"
#include "PokemonSV_IsolatedHuntOptions_Tests.h"
#include "PokemonSV_IsolatedHuntEncounter_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{



namespace{
class Test_IsolatedHunt : public UnitTest{
public:
    Test_IsolatedHunt(std::string name, UnitTestResult (*test)())
        : UnitTest(std::move(name)), m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_test();
    }
private:
    UnitTestResult (*m_test)();
};
}

void add_tests(UnitTestDatabase& database){
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/Flight", test_isolated_hunt_flight);
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/Power", test_isolated_hunt_power);
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/Plan", test_isolated_hunt_plan);
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/Locations", test_isolated_hunt_locations);
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/Loop", test_isolated_hunt_loop);
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/Corridor", test_isolated_hunt_corridor);
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/ExitPolicy", test_isolated_hunt_exit_policy);
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/Options", test_isolated_hunt_options);
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/Sandwich", test_isolated_hunt_sandwich);
    database.add<Test_IsolatedHunt>("PokemonSV/IsolatedHunt/EncounterOptions", test_isolated_hunt_encounter_options);
    database.add<Test_IsolatedHunt>("PokemonSV/Sandwich/Eating", test_sandwich_eating);
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
