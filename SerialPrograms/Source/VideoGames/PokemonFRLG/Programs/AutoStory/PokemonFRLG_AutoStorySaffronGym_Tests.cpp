/*  AutoStory Saffron Gym Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 18.01 and 18.02 (StoryData/frlg-18.0[12].plan.json) on the overworld
 *  sim: Saffron Gym's teleport pads, from the door to the tile below Sabrina
 *  and back out to Saffron's Center, with no trainer met on the way.
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
#include "PokemonFRLG_AutoStoryRows18_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySaffronGymInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStorySaffronGymExpectation{
    std::string_view description;
};

struct AutoStorySaffronGymCase{
    AutoStorySaffronGymInput input;
    AutoStorySaffronGymExpectation expectation;
};

class Test_AutoStorySaffronGym : public UnitTest{
public:
    explicit Test_AutoStorySaffronGym(AutoStorySaffronGymCase test_case)
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
    AutoStorySaffronGymCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  Where each of `row`'s pad steps (the STEP steps, which are only ever
//  onto a pad here) leaves the player, in order, after running the row's
//  steps on the sim up to (not including) its Center step. Fails with the
//  row's message if a step does not run.
struct PadWalk{
    std::string failure;
    std::vector<MapPosition> landings;
    MapPosition end;
    bool clean = false;
};

PadWalk walk_pads(const StoryCheckpoint& row){
    PadWalk walk;
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row.steps){
        if (step.action == CENTER_ENTER_VARIANT){
            break;
        }
        if (step.kind == StoryStepKind::GAME || step.kind == StoryStepKind::TALK || step.kind == StoryStepKind::BATTLE){
            continue;
        }
        walk.failure = row_step_on_sim(sim, at, step, row.id);
        if (!walk.failure.empty()){
            break;
        }
        if (step.kind == StoryStepKind::STEP){
            walk.landings.push_back(sim.state().position);
        }
    }
    walk.end = sim.state().position;
    walk.clean = sim.state().trainers_beaten.empty() && sim.state().violations.empty();
    return walk;
}

//  The pads, in the order the way in takes them, and where each leaves the
//  player: (18, 20) to (28, 23), (20, 20) to (28, 4), (20, 7) to (0, 4),
//  (0, 7) to (18, 15) (SaffronCity_Gym map.json warps 3, 24, 22 and 5).
UnitTestResult test_pads_to_sabrina(){
    const PadWalk walk = walk_pads(checkpoint("18.01"));
    if (!walk.failure.empty()){
        return walk.failure;
    }
    const std::vector<MapPosition> expected{
        {"SaffronCity_Gym", {28, 23}}, {"SaffronCity_Gym", {28, 4}}, {"SaffronCity_Gym", {0, 4}}, {"SaffronCity_Gym", {18, 15}},
    };
    if (walk.landings != expected){
        return "18.01: the pads did not land on (28, 23), (28, 4), (0, 4) and (18, 15), in that order.";
    }
    const MapPosition below_sabrina{"SaffronCity_Gym", {14, 12}};
    if (!(walk.end == below_sabrina)){
        return std::format("18.01: ended on {}, not below Sabrina {}.", to_string(walk.end), to_string(below_sabrina));
    }
    if (!walk.clean){
        return "18.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  The way out: the same pads the other way, each pair linked both ways,
//  then the door onto Saffron City and the street to below the Center's door.
UnitTestResult test_pads_out_to_center(){
    const PadWalk walk = walk_pads(checkpoint("18.02"));
    if (!walk.failure.empty()){
        return walk.failure;
    }
    const std::vector<MapPosition> expected{
        {"SaffronCity_Gym", {0, 7}}, {"SaffronCity_Gym", {20, 7}}, {"SaffronCity_Gym", {20, 20}}, {"SaffronCity_Gym", {18, 20}},
    };
    if (walk.landings != expected){
        return "18.02: the pads did not land on (0, 7), (20, 7), (20, 20) and (18, 20), in that order.";
    }
    const MapPosition below_center{"SaffronCity", {24, 39}};
    if (!(walk.end == below_center)){
        return std::format("18.02: ended on {}, not below Saffron's Center door {}.", to_string(walk.end), to_string(below_center));
    }
    if (!walk.clean){
        return "18.02: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  The control for the two above: the sim does see Cameron (24, 18). Row 21,
//  where the planner would walk (20, 21) from (28, 23) if the row did not
//  stop on row 22 first, is in his line (24, 19 to 21), so a walk along it
//  is seen and fails.
UnitTestResult test_cameron_sees_row_21(){
    OverworldSim sim(story_maps(), MapPosition{"SaffronCity_Gym", {28, 21}}, Direction::WEST);
    StoryPose at{"SaffronCity_Gym", {28, 21}, Direction::WEST};
    StoryStep along;
    along.kind = StoryStepKind::ROUTE;
    along.name = "along_row_21";
    along.args = {{"target", Pokemon::GridPose{"SaffronCity_Gym", {{20, 21}}, Direction::WEST}}, {"fight_wild", false}};
    const std::string failure = row_step_on_sim(sim, at, along, "18.01");
    if (failure.empty() && sim.state().trainers_beaten.empty() && sim.state().violations.empty()){
        return "A walk along row 21 was not seen by Cameron (24, 18).";
    }
    return true;
}

const AutoStorySaffronGymCase AUTO_STORY_SAFFRON_GYM_CASES[] = {
    {
        .input = {.name = "18.01 Saffron Gym's pads from the door to below Sabrina", .body = test_pads_to_sabrina},
        .expectation = {.description = "four pads, landing in the rooms in order, below Sabrina, nobody met"},
    },
    {
        .input = {.name = "18.02 Saffron Gym's pads back out to the Center's door", .body = test_pads_out_to_center},
        .expectation = {.description = "four pads the other way, below Saffron's Center door, nobody met"},
    },
    {
        .input = {.name = "Saffron Gym: Cameron sees a walk along row 21", .body = test_cameron_sees_row_21},
        .expectation = {.description = "the sim meets a trainer on row 21, so the rows' row 22 is a real detour"},
    },
};

} // namespace


void add_tests_AutoStorySaffronGym(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SAFFRON_GYM_CASES){
        database.add<Test_AutoStorySaffronGym>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
