/*  AutoStory Surf Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 12.03's water (StoryData/frlg-12.03.plan.json) on the overworld sim:
 *  each leg on Route 21's water lands on a sand bar, an island or the shore
 *  and ends at its bank, or at a wall, never on open water, where a stop
 *  part-way can't be placed (console B, 2026-11-13 04:15, 06:55 and 07:02).
 *  The faults seen there are injected at every step on the water.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const MapPosition BELOW_CINNABAR_CENTER{"CinnabarIsland", {14, 12}};

const StoryCheckpoint& row_12_03(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& data){ return data.id == "12.03"; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint 12.03.");
    }
    return *row;
}

//  A table pose as a StoryPose; FRLG's table has one-tile poses only.
StoryPose one_tile(const Pokemon::GridPose& grid_pose){
    const std::optional<StoryPose> ret = story_pose(Pokemon::pose_of(grid_pose));
    if (!ret){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Not a one-tile pose.");
    }
    return *ret;
}

//  A leg on the water that ends where the player can be found again: at a
//  wall (the bank of a sand bar it lands on, or the map's edge) or a warp.
bool ends_found(const RouteStep& step){
    return step.action != RouteAction::WALK || !step.surfing || step.to_wall || step.warps;
}

//  What a 12.03 run on the sim did: its failure (empty if none), where it
//  ended, the steps it surfed and the legs it planned on the water that
//  end in the open.
struct SurfRun{
    std::string failure;
    MapPosition end;
    int surf_steps = 0;
    std::vector<int> surf_holds;
    std::vector<std::string> open_water_legs;
    std::vector<std::string> trainers;
};

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n  " + line;
    }
    return text;
}

//  12.03 on a sim with `script`, from Fuchsia's nurse up to the Center's
//  door on Cinnabar, as test_fuchsia_to_cinnabar() runs it. Each ROUTE
//  step is also planned from where the sim stands, for its water legs.
SurfRun run_12_03(SimScript script = {}){
    const StoryCheckpoint& row = row_12_03();
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing, std::move(script));
    SurfRun run;
    for (const StoryStep& step : row.steps){
        if (step.action == CENTER_ENTER_VARIANT){
            break;
        }
        if (step.kind == StoryStepKind::ROUTE){
            const StoryPose target = one_tile(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target"));
            const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
                story_maps(), sim.state().position, RouteGoal{{target.map, target.tile}, target.facing},
                story_route_costs(), sim.state().facing
            );
            for (const RouteStep& planned : route.value_or(std::vector<RouteStep>())){
                if (!ends_found(planned)){
                    run.open_water_legs.push_back(step.name + ": " + to_string(planned));
                }
            }
        }
        run.failure = row_step_on_sim(sim, at, step, row.id);
        if (!run.failure.empty()){
            break;
        }
    }
    run.end = sim.state().position;
    run.surf_steps = sim.state().surf_steps;
    run.surf_holds = sim.state().surf_holds;
    if (run.failure.empty() && !sim.state().violations.empty()){
        run.failure = "a violation: " + sim.state().violations.back();
    }
    run.trainers = sim.state().trainers_beaten;
    return run;
}

//  No leg on Route 21's water ends in the open: each lands and is held to
//  the bank, or ends at a wall. Before, "south 15 (open)" from (7, 0) was
//  stopped 8 tiles in by a message, and the player wasn't found again
//  (console B, 2026-11-13 06:55 and 07:02).
UnitTestResult test_no_open_water_legs(){
    const SurfRun run = run_12_03();
    if (!run.failure.empty()){
        return "12.03 failed on the sim: " + run.failure;
    }
    if (run.surf_steps == 0){
        return "12.03 surfed no step on the sim.";
    }
    if (!run.open_water_legs.empty()){
        return "12.03 plans legs on the water that end in the open:" + joined(run.open_water_legs);
    }
    //  Swimmer Roland wanders Route21_South's (8..12, 27..31) and sees 4
    //  tiles each way: every way from sand bar to sand bar south of
    //  (5..11, 21..25) crosses his sight. He is the one trainer fought.
    if (run.trainers.size() > 1){
        return "12.03 meets more than Swimmer Roland:" + joined(run.trainers);
    }
    return true;
}

//  A message stops the hold after every step on the water in turn, as
//  "a message" did 8 tiles into "south 15 (open)" (console B, 2026-11-13
//  06:55 and 07:02): each run still ends below Cinnabar's Center door, the
//  leg held on to its bank after the message.
UnitTestResult test_message_at_every_surf_step(){
    const SurfRun clean = run_12_03();
    if (!clean.failure.empty()){
        return "12.03 failed on the sim: " + clean.failure;
    }
    std::vector<std::string> failed;
    for (int step = 1; step <= clean.surf_steps; step++){
        SimScript script;
        script.messages_at_surf_steps = {step};
        const SurfRun run = run_12_03(std::move(script));
        if (!run.failure.empty() || !(run.end == BELOW_CINNABAR_CENTER)){
            failed.push_back(std::format(
                "message after surfed step {}: ended on {}{}", step, to_string(run.end),
                run.failure.empty() ? "" : "; " + run.failure
            ));
        }
    }
    if (!failed.empty()){
        return std::format("{} of {} runs with a message on the water fail:{}", failed.size(), clean.surf_steps, joined(failed));
    }
    return true;
}

//  A hold on the water that stops short with nothing seen, as "south 86
//  (open)" did (console B, 2026-11-13 04:15), at each in turn: the run
//  ends below Cinnabar's Center door, or fails (a leg that lands checks the
//  player is on foot); it never goes on from where it wrongly takes the
//  player to be and ends elsewhere.
UnitTestResult test_short_hold_never_goes_on_from_a_guess(){
    const SurfRun clean = run_12_03();
    if (!clean.failure.empty()){
        return "12.03 failed on the sim: " + clean.failure;
    }
    std::vector<std::string> wrong;
    if (clean.surf_holds.empty()){
        return "12.03 held no leg on the water on the sim.";
    }
    for (const int hold : clean.surf_holds){
        SimScript script;
        script.short_holds = {{hold, 3}};
        const SurfRun run = run_12_03(std::move(script));
        if (run.failure.empty() && !(run.end == BELOW_CINNABAR_CENTER)){
            wrong.push_back(std::format("hold {} 3 tiles short: passed, ended on {}", hold, to_string(run.end)));
        }
        if (run.failure.find("trainer") != std::string::npos || run.failure.find("violation") != std::string::npos){
            wrong.push_back(std::format("hold {} 3 tiles short: {}", hold, run.failure));
        }
    }
    if (!wrong.empty()){
        return "12.03 went on from a guess after a short hold:" + joined(wrong);
    }
    return true;
}

struct SurfCase{
    std::string_view name;
    std::function<UnitTestResult()> body;
};

class Test_AutoStorySurf : public UnitTest{
public:
    explicit Test_AutoStorySurf(SurfCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Surf: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    SurfCase m_case;
};

const SurfCase SURF_CASES[] = {
    {"12.03's legs on the water land or end at a wall", test_no_open_water_legs},
    {"12.03 with a message after each step on the water", test_message_at_every_surf_step},
    {"12.03 with a hold stopped short, unseen", test_short_hold_never_goes_on_from_a_guess},
};

} // namespace


void add_tests_AutoStorySurf(UnitTestDatabase& database){
    for (const SurfCase& item : SURF_CASES){
        database.add<Test_AutoStorySurf>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
