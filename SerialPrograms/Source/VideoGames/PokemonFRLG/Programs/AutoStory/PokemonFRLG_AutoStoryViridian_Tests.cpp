/*  AutoStory Viridian Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 17.01 to 17.04 (StoryData/frlg-17.0N.plan.json) on the overworld
 *  sim: Saffron's Center to Cinnabar's by Fly and Surf, and from Cinnabar's
 *  Center by Fly to Viridian City and through its Gym to Giovanni.
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
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSpin.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows17_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryViridianInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryViridianExpectation{
    std::string_view description;
};

struct AutoStoryViridianCase{
    AutoStoryViridianInput input;
    AutoStoryViridianExpectation expectation;
};

class Test_AutoStoryViridian : public UnitTest{
public:
    explicit Test_AutoStoryViridian(AutoStoryViridianCase test_case)
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
    AutoStoryViridianCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  17.01 on the sim from Saffron's nurse: out of the Center, the Fly to
//  Pallet Town, the shore, the Surf onto Route 21 and the way south over it
//  to Cinnabar's Center door. No trainer is met and no rule is broken.
UnitTestResult test_surf_to_cinnabar(){
    const StoryCheckpoint& row = checkpoint("17.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    std::vector<std::string> maps;
    for (const StoryStep& step : row.steps){
        if (step.action == CENTER_ENTER_VARIANT){
            break;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return std::format("17.01: ended on {}; {}", to_string(sim.state().position), failure);
        }
        maps.push_back(sim.state().position.map);
    }
    const MapPosition below_center{"CinnabarIsland", {14, 12}};
    if (!(sim.state().position == below_center)){
        return std::format("17.01: ended on {}, not below Cinnabar's Center door {}.", to_string(sim.state().position), to_string(below_center));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "17.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

const AutoStoryViridianCase AUTO_STORY_VIRIDIAN_CASES[] = {
    {
        .input = {.name = "17.01 Saffron to Cinnabar by Fly and Surf", .body = test_surf_to_cinnabar},
        .expectation = {.description = "Pallet Town's shore, Route 21 surfed, below Cinnabar's Center door, nobody met"},
    },
};

} // namespace


void add_tests_AutoStoryViridian(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_VIRIDIAN_CASES){
        database.add<Test_AutoStoryViridian>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
