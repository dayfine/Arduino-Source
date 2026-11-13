/*  AutoStory Victory Road Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 19.01 and after (StoryData/frlg-19.0N.plan.json) on the overworld
 *  sim: Viridian's Center to Route 22's rival, Route 23, Victory Road and
 *  Indigo Plateau's Center.
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
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows19_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryVictoryRoadInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryVictoryRoadExpectation{
    std::string_view description;
};

struct AutoStoryVictoryRoadCase{
    AutoStoryVictoryRoadInput input;
    AutoStoryVictoryRoadExpectation expectation;
};

class Test_AutoStoryVictoryRoad : public UnitTest{
public:
    explicit Test_AutoStoryVictoryRoad(AutoStoryVictoryRoadCase test_case)
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
    AutoStoryVictoryRoadCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  Runs the walking steps of `row` on the sim from its start, up to (not
//  including) the step named `until`; the steps that are not walks (the
//  Mart, the level check, the battle) are the console's.
std::string walk_row_until(OverworldSim& sim, StoryPose& at, const StoryCheckpoint& row, std::string_view until){
    for (const StoryStep& step : row.steps){
        if (step.name == until){
            return "";
        }
        if (step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS && step.kind != StoryStepKind::STEP){
            continue;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
    }
    return std::format("{}: no step {}.", row.id, until);
}

//  19.01: the street to Route 22 and along it to the rival's trigger column,
//  then the step onto it. The rival is the battle step's, so the walk ends
//  on the trigger tile (33, 5), facing west, with no trainer met.
UnitTestResult test_route22_rival_trigger(){
    const StoryCheckpoint& row = checkpoint("19.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_row_until(sim, at, row, "rival_route22"); !failure.empty()){
        return failure;
    }
    const MapPosition trigger{"Route22", {33, 5}};
    if (!(sim.state().position == trigger)){
        return std::format("19.01: ended on {}, not the rival's trigger {}.", to_string(sim.state().position), to_string(trigger));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "19.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

const AutoStoryVictoryRoadCase AUTO_STORY_VICTORY_ROAD_CASES[] = {
    {
        .input = {.name = "19.01 Viridian to the Route 22 rival's trigger", .body = test_route22_rival_trigger},
        .expectation = {.description = "Route 22 (33, 5), no trainer met"},
    },
};

} // namespace


void add_tests_AutoStoryVictoryRoad(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_VICTORY_ROAD_CASES){
        database.add<Test_AutoStoryVictoryRoad>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
