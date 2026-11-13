/*  AutoStory Mansion Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 13.01 to 13.04 (StoryData/frlg-13.0N.plan.json) on the overworld
 *  sim: Cinnabar's Center to the Pokemon Mansion's statue switches, the
 *  SECRET KEY and the Escape Rope out. The sim reads the real maps through
 *  the switch's state, from the checkpoint's start_gates, and the routes
 *  press the statues themselves.
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
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalkTimes.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows13_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryMansionInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryMansionExpectation{
    std::string_view description;
};

struct AutoStoryMansionCase{
    AutoStoryMansionInput input;
    AutoStoryMansionExpectation expectation;
};

class Test_AutoStoryMansion : public UnitTest{
public:
    explicit Test_AutoStoryMansion(AutoStoryMansionCase test_case)
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
    AutoStoryMansionCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  Burglar Lewis, B1F's wanderer: he walks (19..21, 24..26) with sight 1 and
//  looks every way, so the sim, which takes his whole range as seen, meets
//  him where 13.03 passes (22, 24). Whether he is there is up to the dice;
//  the battle code fights him if he sees the player. Nobody else is met.
const std::string LEWIS = "poke_maniac";

struct MansionRow{
    std::string_view id;
    MapPosition end;
    std::vector<std::string> floors;
};

//  A row on the sim from its start, through the gates it has on a save
//  (start_gates): every step lands (a route presses the statues it needs),
//  the row ends on its postcondition's tile after the floors it should
//  cross, the switch is as its last step says, and nobody but Lewis is met
//  and no rule is broken on the way (checked after each step).
UnitTestResult test_row(const MansionRow& expected){
    const StoryCheckpoint& row = checkpoint(expected.id);
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    SimScript script;
    script.gates = row.start_gates.value_or(Pokemon::GateState());
    std::optional<OverworldSim> sim;
    sim.emplace(story_maps(), MapPosition{at.map, at.tile}, *at.facing, script);
    std::vector<std::string> floors;
    for (const StoryStep& step : row.steps){
        if (step.name == "escape_rope"){
            //  The rope's script has no map: the player stands where it says.
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            sim.emplace(story_maps(), MapPosition{at.map, at.tile}, *at.facing, script);
        }else if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
            return std::format("{}: ended on {}; {}", row.id, to_string(sim->state().position), failure);
        }
        if (!sim->state().violations.empty()){
            return std::format("{}/{}: broke a rule on the way, on {}: {}.", row.id, step.name, to_string(sim->state().position), sim->state().violations.front());
        }
        for (const std::string& trainer : sim->state().trainers_beaten){
            if (trainer != LEWIS){
                return std::format("{}/{}: met a trainer: {}.", row.id, step.name, trainer);
            }
        }
        if (floors.empty() || floors.back() != at.map){
            floors.push_back(at.map);
        }
        if (step.name != "escape_rope"){
            script.gates = sim->state().gates;
        }
    }
    if (!(MapPosition{at.map, at.tile} == expected.end)){
        return std::format("{}: ended on {}, not {}.", row.id, to_string(MapPosition{at.map, at.tile}), to_string(expected.end));
    }
    if (floors != expected.floors){
        std::string crossed;
        for (const std::string& floor : floors){
            crossed += std::format(" {}", floor);
        }
        return std::format("{}: the steps crossed the wrong floors:{}.", row.id, crossed);
    }
    return true;
}

const MansionRow ROW_13_01{"13.01", {"PokemonMansion_3F", {19, 13}},
    {"PokemonMansion_3F"}};
const MansionRow ROW_13_02{"13.02", {"PokemonMansion_B1F", {34, 29}},
    {"PokemonMansion_1F", "PokemonMansion_B1F"}};
const MansionRow ROW_13_03{"13.03", {"PokemonMansion_B1F", {23, 5}},
    {"PokemonMansion_B1F"}};
const MansionRow ROW_13_04{"13.04", {"CinnabarIsland", {8, 4}},
    {"PokemonMansion_B1F", "CinnabarIsland"}};

const AutoStoryMansionCase AUTO_STORY_MANSION_CASES[] = {
    {
        .input = {.name = "13.01 Cinnabar to the Mansion's 3F room of holes", .body = []{ return test_row(ROW_13_01); }},
        .expectation = {.description = "through 1F and 2F to 3F's (19, 13) by a statue press, nobody met"},
    },
    {
        .input = {.name = "13.02 3F's hole, 1F's stairs", .body = []{ return test_row(ROW_13_02); }},
        .expectation = {.description = "through the hole and the stairs to B1F's (34, 29)"},
    },
    {
        .input = {.name = "13.03 B1F's Elixir and TM14", .body = []{ return test_row(ROW_13_03); }},
        .expectation = {.description = "by a statue press to the Elixir and TM14, nobody but Lewis met"},
    },
    {
        .input = {.name = "13.04 the SECRET KEY and the Escape Rope", .body = []{ return test_row(ROW_13_04); }},
        .expectation = {.description = "by a statue press to the key, out by the rope to (8, 4)"},
    },
};

} // namespace


void add_tests_AutoStoryMansion(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_MANSION_CASES){
        database.add<Test_AutoStoryMansion>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
