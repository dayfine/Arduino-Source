/*  AutoStory Saffron Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 15.01 (StoryData/frlg-15.01.plan.json) on the overworld sim: from
 *  Celadon's nurse along Route 7 into its east gate, onto the guard's
 *  trigger, and through Saffron City to its nurse.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows15_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySaffronInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStorySaffronExpectation{
    std::string_view description;
};

struct AutoStorySaffronCase{
    AutoStorySaffronInput input;
    AutoStorySaffronExpectation expectation;
};

class Test_AutoStorySaffron : public UnitTest{
public:
    explicit Test_AutoStorySaffron(AutoStorySaffronCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStorySaffronCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  15.01 on the sim from Celadon's nurse: Route 7, the gate's top trigger
//  (6, 4), where the TEA scene leaves the player, and Saffron City to below
//  its Center's door; the way into the Center, which has no map, is the
//  console's. No trainer is met.
UnitTestResult test_route7_to_saffron(){
    const StoryCheckpoint& row = checkpoint("15.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    const MapPosition trigger{"Route7_EastEntrance", {6, 4}};
    bool on_trigger = false;
    for (const StoryStep& step : row.steps){
        if (step.action == CENTER_ENTER_VARIANT){
            break;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return std::format("15.01: ended on {}; {}", to_string(sim.state().position), failure);
        }
        on_trigger = on_trigger || sim.state().position == trigger;
    }
    if (!on_trigger){
        return std::format("15.01: never stood on the guard's trigger {}.", to_string(trigger));
    }
    const MapPosition below_center{"SaffronCity", {24, 39}};
    if (!(sim.state().position == below_center)){
        return std::format("15.01: ended on {}, not below Saffron's Center door {}.", to_string(sim.state().position), to_string(below_center));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "15.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

const AutoStorySaffronCase AUTO_STORY_SAFFRON_CASES[] = {
    {
        .input = {.name = "15.01 Route 7's gate with the TEA to Saffron's Center", .body = test_route7_to_saffron},
        .expectation = {.description = "onto the guard's trigger, below Saffron's Center door, nobody met"},
    },
};

} // namespace


void add_tests_AutoStorySaffron(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SAFFRON_CASES){
        database.add<Test_AutoStorySaffron>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
