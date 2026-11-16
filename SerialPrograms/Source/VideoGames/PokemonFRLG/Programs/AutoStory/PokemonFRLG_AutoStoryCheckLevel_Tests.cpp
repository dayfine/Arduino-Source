/*  AutoStory Check Level Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "PokemonFRLG_AutoStoryCheckLevel_Tests.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::StoryCheckLevel;

//  The story maps with Viridian Gym's trainers beaten, as 17.04b walks out.
std::vector<MapData> beaten_gym_maps(){
    std::vector<MapData> maps = story_maps();
    for (MapData& map : maps){
        if (map.name != "ViridianCity_Gym"){
            continue;
        }
        for (MapObject& object : map.objects){
            object.trainer.reset();
        }
    }
    return maps;
}

const StoryCheckpoint& checkpoint_row(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [id](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return *row;
}

struct GiovanniRun{
    bool completed = false;
    std::vector<std::string> lines;
    std::vector<RowStubs> stubs;
    //  The stubbed steps that are checks `level` doesn't run.
    std::vector<std::string> skipped_checks_run;
};

//  17.04 (the level check before Giovanni, his battle) and 17.04b (out of
//  the gym, Fly to the Center) at `level`, each saving at its end.
GiovanniRun run_giovanni_rows(StoryCheckLevel level){
    const std::vector<MapData> maps = beaten_gym_maps();
    StoryMapsSim sim(StorySimKnobs{.maps = &maps, .checks = level});
    const std::vector<const StoryCheckpoint*> rows = {&checkpoint_row("17.04"), &checkpoint_row("17.04b")};
    Pokemon::Segment segment;
    segment.id = "17";
    for (const StoryCheckpoint* data : rows){
        segment.checkpoints.push_back(Pokemon::CheckpointSpec{
            .id = data->id, .title = data->title, .saves = true,
            .build = [&sim, data]{ return Pokemon::build_story_checkpoint(*data, sim); },
        });
    }
    GiovanniRun run;
    Pokemon::SegmentHooks hooks;
    hooks.save = [](Pokemon::RunState&){ return Pokemon::outcome_ok(); };
    hooks.reload = [](Pokemon::RunState&){ return Pokemon::outcome_ok(); };
    hooks.log = [&run](const std::string& line){ run.lines.push_back(line); };
    Pokemon::RunState state;
    Pokemon::SegmentPolicy policy;
    policy.attempts_per_checkpoint = 1;
    run.completed = Pokemon::run_segment({.from = "17.04", .to = "17.04b"}, policy, segment, hooks, state).completed;
    run.stubs = sim.stubs().rows();
    for (const RowStubs& stubbed : run.stubs){
        for (const StoryCheckpoint* data : rows){
            for (const StoryStep& step : data->steps){
                const Pokemon::StoryAction* action = sim.actions().find(step);
                const bool was_stubbed = data->id == stubbed.row && std::ranges::find(stubbed.steps, step.name) != stubbed.steps.end();
                if (was_stubbed && action != nullptr && action->check && !Pokemon::runs_check(level, *action->check)){
                    run.skipped_checks_run.push_back(std::format("{}/{}", data->id, step.name));
                }
            }
        }
    }
    return run;
}

bool stubbed(const GiovanniRun& run, std::string_view row, std::string_view step){
    return std::ranges::any_of(run.stubs, [&](const RowStubs& stubs){
        return stubs.row == row && std::ranges::find(stubs.steps, std::string(step)) != stubs.steps.end();
    });
}

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += std::format("\n    {}", line);
    }
    return text;
}


struct CheckLevelCase{
    struct Input{
        StoryCheckLevel level;
    };
    struct Expectation{
        //  Whether 17.04's lead_lv50_for_giovanni (a PARTY_STATS check, a
        //  summary read on the console) reaches the console's binding.
        bool level_check_runs;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const CheckLevelCase CHECK_LEVEL_CASES[] = {
    {
        .name = "17.04 and 17.04b at Default read no summary and no Bag",
        .input = {.level = StoryCheckLevel::DEFAULT},
        .expectation = {.level_check_runs = false},
    },
    {
        .name = "17.04 and 17.04b at Thorough check the lead's level",
        .input = {.level = StoryCheckLevel::THOROUGH},
        .expectation = {.level_check_runs = true},
    },
};

class Test_AutoStoryCheckLevel : public UnitTest{
public:
    explicit Test_AutoStoryCheckLevel(const CheckLevelCase& scenario)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", scenario.name))
        , m_scenario(scenario)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            const GiovanniRun run = run_giovanni_rows(m_scenario.input.level);
            if (!run.completed){
                return std::format("Expected 17.04 and 17.04b through:{}", joined(run.lines));
            }
            if (!run.skipped_checks_run.empty()){
                return std::format("Expected no check this level skips to run:{}", joined(run.skipped_checks_run));
            }
            if (stubbed(run, "17.04", "lead_lv50_for_giovanni") != m_scenario.expectation.level_check_runs){
                return std::format("Expected 17.04's level check {}.", m_scenario.expectation.level_check_runs ? "run" : "skipped");
            }
            if (!stubbed(run, "17.04", "giovanni")){
                return "Expected Giovanni's battle at every level.";
            }
            return true;
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    CheckLevelCase m_scenario;
};

} // namespace


void add_tests_AutoStoryCheckLevel(UnitTestDatabase& database){
    for (const CheckLevelCase& scenario : CHECK_LEVEL_CASES){
        database.add<Test_AutoStoryCheckLevel>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
