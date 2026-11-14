/*  AutoStory Vermilion Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 00.21 (Cerulean to Vermilion City) and 00.22 (the S.S. Anne) on
 *  the overworld sim: their walks reach their ends, and meet only the
 *  trainers the table fights.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryVermilionInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryVermilionExpectation{
};

struct AutoStoryVermilionCase{
    AutoStoryVermilionInput input;
    AutoStoryVermilionExpectation expectation;
};

class Test_AutoStoryVermilion : public UnitTest{
public:
    explicit Test_AutoStoryVermilion(AutoStoryVermilionCase test_case)
        : UnitTest("PokemonFRLG::AutoStory - Simulation: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryVermilionCase m_case;
};

const StoryCheckpoint* checkpoint(const std::string& id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& data){ return data.id == id; });
    return row == rows.end() ? nullptr : &*row;
}

std::string beaten_list(const OverworldSim& sim){
    std::string text;
    for (const std::string& name : sim.state().trainers_beaten){
        text += " " + name;
    }
    return text.empty() ? " none" : text;
}

size_t step_index(const StoryCheckpoint& row, const std::string& name){
    const auto step = std::find_if(row.steps.begin(), row.steps.end(), [&](const StoryStep& data){ return data.name == name; });
    if (step == row.steps.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No step " + row.id + "/" + name + ".");
    }
    return step - row.steps.begin();
}

//  Runs row 00.21's steps from `first` to its end on `sim` from `at`, and
//  checks that only Jeff ("camper") was fought and the walk ended at
//  Vermilion's nurse.
std::string run_to_vermilion(OverworldSim& sim, StoryPose at, const StoryCheckpoint& row, size_t first){
    for (size_t c = first; c < row.steps.size(); c++){
        const std::string failure = row_step_on_sim(sim, at, row.steps[c], row.id);
        if (!failure.empty()){
            return failure;
        }
    }
    if (sim.state().trainers_beaten != std::vector<std::string>{"camper"} || !sim.state().violations.empty()){
        return "00.21: expected Jeff alone beaten and no violation; beat" + beaten_list(sim) + ".";
    }
    if (!(sim.state().position == MapPosition{"VermilionCity_PokemonCenter_1F", {7, 4}})){
        return "00.21: expected to end at Vermilion's nurse, not " + to_string(sim.state().position) + ".";
    }
    return "";
}

//  00.21 on the sim, from Cerulean's Pokemon Center through the burgled
//  house to (32, 7), west of the Rocket, meeting no one; his trigger and
//  battle aren't the sim's, so from where his battle leaves the player:
//  Route 5, the Underground Path and Route 6, down column 15 out of
//  Keigo's and Elijah's lines, round Isabelle's row to Jeff, who is fought
//  on the step onto (10, 33), and on to Vermilion's nurse. Then from the
//  walk onto Route 6's column 15 ended a tile short, on column 16: the
//  same.
UnitTestResult test_route6_jeff_only(){
    const StoryCheckpoint* row = checkpoint("00.21");
    if (row == nullptr){
        return "No checkpoint 00.21.";
    }
    StoryPose at = *story_pose(Pokemon::pose_of(*row->start));
    OverworldSim house(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    const size_t rocket = step_index(*row, "rocket_trigger");
    for (size_t c = 0; c < rocket; c++){
        if (const std::string failure = row_step_on_sim(house, at, row->steps[c], row->id); !failure.empty()){
            return failure;
        }
    }
    if (!(house.state().position == MapPosition{"CeruleanCity", {32, 7}}) || !house.state().trainers_beaten.empty()){
        return std::format("00.21: expected to reach (32, 7) west of the Rocket, meeting no one, not {}.", to_string(house.state().position));
    }
    const size_t battle = step_index(*row, "rocket_grunt");
    const StoryPose after_rocket = *story_pose(Pokemon::pose_of(*row->steps[battle].pose_after));
    OverworldSim whole(story_maps(), MapPosition{after_rocket.map, after_rocket.tile}, *after_rocket.facing);
    if (const std::string failure = run_to_vermilion(whole, after_rocket, *row, battle + 1); !failure.empty()){
        return "From the Rocket: " + failure;
    }
    const size_t walk = step_index(*row, "route6_column15");
    const StoryPose column = *story_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(row->steps[walk].args, "target")));
    OverworldSim sim(story_maps(), MapPosition{column.map, {column.tile.x + 1, column.tile.y}}, Direction::WEST);
    if (const std::string failure = run_to_vermilion(sim, column, *row, walk + 1); !failure.empty()){
        return "A tile short: " + failure;
    }
    return true;
}

//  Whether `route` from `start` steps onto any of `tiles` of `map`.
bool steps_onto(const std::vector<RouteStep>& route, MapPosition start, const std::string& map, const std::vector<TilePos>& tiles){
    for (const RouteStep& step : route){
        const std::vector<MapPosition> passed = step.action == RouteAction::WALK ? leg_tiles(story_maps(), start, step) : std::vector<MapPosition>{};
        for (size_t c = 1; c < passed.size(); c++){
            if (passed[c].map == map && std::find(tiles.begin(), tiles.end(), passed[c].point) != tiles.end()){
                return true;
            }
        }
        start = step.end;
    }
    return false;
}

//  Where the table says the player stands after `step` from `at`.
StoryPose pose_after(const StoryPose& at, const StoryStep& step){
    if (step.pose_after){
        return *story_pose(Pokemon::pose_of(*step.pose_after));
    }
    if (step.kind == StoryStepKind::STEP){
        return StoryPose{at.map, at.tile.next(Pokemon::story_arg<Direction>(step.args, "direction")), Pokemon::story_arg<Direction>(step.args, "direction")};
    }
    return at;
}

//  00.22's walks on the sim, each from where the table says it starts: to
//  the harbor short of the ticket check, aboard and along 2F short of the
//  rival's trigger row (30..32, 6), into the Captain's office behind him,
//  back to 1F's door, and from the pier to the nurse. None meets a
//  trainer: the ship's are in its cabins and on its deck.
UnitTestResult test_ss_anne_walks(){
    const StoryCheckpoint* row = checkpoint("00.22");
    if (row == nullptr){
        return "No checkpoint 00.22.";
    }
    StoryPose at = *story_pose(Pokemon::pose_of(*row->start));
    bool before_rival = true;
    for (const StoryStep& step : row->steps){
        if (step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS){
            before_rival &= step.kind != StoryStepKind::BATTLE;
            at = pose_after(at, step);
            continue;
        }
        const std::optional<std::vector<RouteStep>> route = walked_route(at, step);
        if (!route || route->empty()){
            return "00.22/" + step.name + ": no route.";
        }
        const MapPosition start{at.map, at.tile};
        if (steps_onto(*route, start, "VermilionCity", {{22, 33}, {23, 33}})
            || (before_rival && steps_onto(*route, start, "SSAnne_2F_Corridor", {{30, 6}, {31, 6}, {32, 6}}))
        ){
            return "00.22/" + step.name + ": crosses a trigger before its step.";
        }
        OverworldSim sim(story_maps(), start, *at.facing);
        if (const std::string failure = row_step_on_sim(sim, at, step, row->id); !failure.empty()){
            return failure;
        }
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return "00.22/" + step.name + ": ended on " + to_string(sim.state().position) + ", not " + to_string(MapPosition{at.map, at.tile}) + ".";
        }
        if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
            return "00.22/" + step.name + ": a trainer's battle or a violation on the way.";
        }
    }
    if (!(MapPosition{at.map, at.tile} == MapPosition{"VermilionCity_PokemonCenter_1F", {7, 4}})){
        return "00.22: expected to end at Vermilion's nurse.";
    }
    return true;
}

} // namespace


const AutoStoryVermilionCase AUTO_STORY_VERMILION_CASES[] = {
    {
        .input = {"00.21 to Vermilion, only Jeff on Route 6, the walk onto column 15 whole or a tile short", test_route6_jeff_only},
        .expectation = {},
    },
    {
        .input = {"00.22 the S.S. Anne's walks, short of the ticket check and the rival's trigger", test_ss_anne_walks},
        .expectation = {},
    },
};

void add_tests_AutoStoryVermilion(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_VERMILION_CASES){
        database.add<Test_AutoStoryVermilion>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
