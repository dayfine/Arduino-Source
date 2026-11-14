/*  AutoStory Silph Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 16.01 (StoryData/frlg-16.01.plan.json) on the overworld sim: from
 *  Saffron's nurse into Silph Co., up to 3F and through its pad to 5F.
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
#include "PokemonFRLG_AutoStoryRows16_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySilphInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStorySilphExpectation{
    std::string_view description;
};

struct AutoStorySilphCase{
    AutoStorySilphInput input;
    AutoStorySilphExpectation expectation;
};

class Test_AutoStorySilph : public UnitTest{
public:
    explicit Test_AutoStorySilph(AutoStorySilphCase test_case)
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
    AutoStorySilphCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  16.01 on the sim from Saffron's nurse: Silph Co.'s stairs to 3F, west
//  along the hall and onto the pad to 5F, where the player stands on the pad
//  that leads back. No trainer is met and no rule is broken.
UnitTestResult test_pad_to_5f(){
    const StoryCheckpoint& row = checkpoint("16.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    std::vector<std::string> floors;
    for (const StoryStep& step : row.steps){
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return std::format("16.01: ended on {}; {}", to_string(sim.state().position), failure);
        }
        floors.push_back(sim.state().position.map);
    }
    const MapPosition pad{"SilphCo_5F", {15, 7}};
    if (!(sim.state().position == pad)){
        return std::format("16.01: ended on {}, not on 5F's pad {}.", to_string(sim.state().position), to_string(pad));
    }
    if (floors != std::vector<std::string>{"SilphCo_3F", "SilphCo_5F"}){
        return "16.01: the steps ended on the wrong floors.";
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "16.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

const AutoStorySilphCase AUTO_STORY_SILPH_CASES[] = {
    {
        .input = {.name = "16.01 Silph Co.'s stairs to 3F and its pad to 5F", .body = test_pad_to_5f},
        .expectation = {.description = "to 3F's pad (4, 6), onto 5F's (15, 7), nobody met"},
    },
};

} // namespace


void add_tests_AutoStorySilph(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SILPH_CASES){
        database.add<Test_AutoStorySilph>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
