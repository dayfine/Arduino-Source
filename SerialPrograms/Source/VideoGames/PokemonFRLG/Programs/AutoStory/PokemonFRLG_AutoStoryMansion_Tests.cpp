/*  AutoStory Mansion Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 13.01 to 13.04 (StoryData/frlg-13.0N.plan.json) on the overworld
 *  sim: Cinnabar's Center to the Pokemon Mansion's statue switches, the
 *  SECRET KEY and the Escape Rope out. The sim has the map's switch state
 *  (flag clear), so a step through a barrier the switches open is declared.
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

struct MansionRow{
    std::string_view id;
    MapPosition end;
    std::vector<std::string_view> floors;
};

//  A row on the sim from its start: every step lands, the row ends on its
//  postcondition's tile after the floors it should cross, and nobody is met
//  and no rule is broken on the way.
UnitTestResult test_row(const MansionRow& expected){
    const StoryCheckpoint& row = checkpoint(expected.id);
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    std::vector<std::string_view> floors;
    for (const StoryStep& step : row.steps){
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return std::format("{}: ended on {}; {}", row.id, to_string(sim.state().position), failure);
        }
        if (floors.empty() || floors.back() != sim.state().position.map){
            floors.push_back(sim.state().position.map);
        }
    }
    if (!(sim.state().position == expected.end)){
        return std::format("{}: ended on {}, not {}.", row.id, to_string(sim.state().position), to_string(expected.end));
    }
    if (floors != expected.floors){
        return std::format("{}: the steps crossed the wrong floors.", row.id);
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return std::format("{}: met a trainer or broke a rule on the way.", row.id);
    }
    return true;
}

const MansionRow ROW_13_01{"13.01", {"PokemonMansion_3F", {12, 6}},
    {"CinnabarIsland", "PokemonMansion_1F", "PokemonMansion_2F", "PokemonMansion_3F"}};
const MansionRow ROW_13_02{"13.02", {"PokemonMansion_B1F", {34, 29}},
    {"PokemonMansion_3F", "PokemonMansion_1F", "PokemonMansion_B1F"}};
const MansionRow ROW_13_03{"13.03", {"PokemonMansion_B1F", {27, 6}},
    {"PokemonMansion_B1F"}};
const MansionRow ROW_13_04{"13.04", {"CinnabarIsland", {8, 4}},
    {"PokemonMansion_B1F", "CinnabarIsland"}};

const AutoStoryMansionCase AUTO_STORY_MANSION_CASES[] = {
    {
        .input = {.name = "13.01 Cinnabar to the Mansion's 3F statue", .body = []{ return test_row(ROW_13_01); }},
        .expectation = {.description = "through 1F and 2F to 3F's (12, 6), nobody met"},
    },
    {
        .input = {.name = "13.02 3F's opening, the hole, 1F's stairs", .body = []{ return test_row(ROW_13_02); }},
        .expectation = {.description = "through the declared opening and the hole to B1F's (34, 29)"},
    },
    {
        .input = {.name = "13.03 B1F's two statues, the Elixir and TM14", .body = []{ return test_row(ROW_13_03); }},
        .expectation = {.description = "through the declared opening to (27, 6), nobody met"},
    },
    {
        .input = {.name = "13.04 the SECRET KEY and the Escape Rope", .body = []{ return test_row(ROW_13_04); }},
        .expectation = {.description = "through the declared opening to the key, out by the rope to (8, 4)"},
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
