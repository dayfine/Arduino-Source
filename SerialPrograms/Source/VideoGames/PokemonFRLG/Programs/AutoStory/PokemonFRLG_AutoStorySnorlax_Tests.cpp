/*  AutoStory Snorlax Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 11.01 on the overworld sim: from Mr. Fuji's house (10.06) out to
 *  Lavender Town, a Fly to Celadon City, west on the Bicycle to Route 16's
 *  Snorlax, saved beside its tile once it is gone. The flute's wild
 *  battle is the script's: the sim takes the table's word for where it ends.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySnorlaxInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStorySnorlaxExpectation{
};

struct AutoStorySnorlaxCase{
    AutoStorySnorlaxInput input;
    AutoStorySnorlaxExpectation expectation;
};

class Test_AutoStorySnorlax : public UnitTest{
public:
    explicit Test_AutoStorySnorlax(AutoStorySnorlaxCase test_case)
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
    AutoStorySnorlaxCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return *row;
}

//  The maps as the story leaves them by 11.01 (as the Tower's rows), the
//  Snorlax still on Route 16.
std::vector<MapData> maps_before_flute(){
    const std::vector<MapData> all = overworld_maps();
    std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
    flags.merge(map_flag_set(all, "new_game"));
    flags.erase("FLAG_HIDE_VERMILION_CITY_OAKS_AIDE");
    flags.erase("FLAG_HIDE_ROUTE_16_SNORLAX");
    return overworld_maps(flags);
}

//  The Snorlax's battle: a BATTLE step with no trainer, which the sim
//  can't play; the table says where it ends.
bool scripted(const StoryStep& step){
    return step.kind == StoryStepKind::BATTLE && Pokemon::story_arg<std::string>(step.args, "trainer").empty();
}

//  11.01: out of Mr. Fuji's house, Fly to Celadon, the Bicycle to the
//  Snorlax with no trainer fought, standing beside its tile after the
//  battle.
UnitTestResult test_snorlax(){
    const StoryCheckpoint& row = checkpoint("11.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    const std::vector<MapData> maps = maps_before_flute();
    if (std::none_of(maps.begin(), maps.end(), [](const MapData& map){
        return map.name == "Route16" && std::any_of(map.objects.begin(), map.objects.end(), [](const MapObject& object){
            return object.x == 31 && object.y == 13;
        });
    })){
        return "11.01: the Snorlax isn't at (31, 13) on Route 16 before the flute.";
    }
    auto sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row.steps){
        if (scripted(step)){
            if (!sim->state().trainers_beaten.empty() || !sim->state().violations.empty()){
                return std::format("11.01: a battle or a violation before {}.", step.name);
            }
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing);
            continue;
        }
        if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
            return std::format("11.01: ended on {}; {}", to_string(sim->state().position), failure);
        }
    }
    if (!sim->state().trainers_beaten.empty() || !sim->state().violations.empty()){
        return "11.01: a battle or a violation after the Snorlax.";
    }
    const MapPosition end{"Route16", {32, 13}};
    if (!(sim->state().position == end)){
        return std::format("11.01: ended on {}, not {}.", to_string(sim->state().position), to_string(end));
    }
    return true;
}

//  11.01's Snorlax battle runs (the "run" arg), and only that battle.
UnitTestResult test_snorlax_battle_runs(){
    for (const StoryStep& step : checkpoint("11.01").steps){
        if (step.kind != StoryStepKind::BATTLE){
            continue;
        }
        if (!Pokemon::story_arg<bool>(step.args, "run")){
            return std::format("11.01: {} fights; expected it to run.", step.name);
        }
    }
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.kind == StoryStepKind::BATTLE && row.id != "11.01" && Pokemon::story_arg<bool>(step.args, "run")){
                return std::format("{}: {} runs; only 11.01's Snorlax should.", row.id, step.name);
            }
        }
    }
    return true;
}

const AutoStorySnorlaxCase AUTO_STORY_SNORLAX_CASES[] = {
    {.input = {"11.01 Fly to Celadon and Route 16's Snorlax", test_snorlax}, .expectation = {}},
    {.input = {"11.01 runs from the Snorlax's battle", test_snorlax_battle_runs}, .expectation = {}},
};

} // namespace


void add_tests_AutoStorySnorlax(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SNORLAX_CASES){
        database.add<Test_AutoStorySnorlax>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
