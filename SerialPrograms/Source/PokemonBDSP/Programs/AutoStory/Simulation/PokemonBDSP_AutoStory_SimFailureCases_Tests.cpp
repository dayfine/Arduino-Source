/*  BDSP AutoStory - Console failure cases on the story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One case per failure console B showed in its events and perception
 *  (#268's design note, the planner note), run on the story simulation
 *  with the knobs that make it happen there (SimWorldKnobs, SimMisses):
 *  walks that slide on onto a warp, fades the walks' watchers miss or read
 *  as a battle, banners missed or gone, dim floors, late first lines and
 *  people who wander.
 *
 *  A case the program fails today is on KNOWN_FAILING with the change that
 *  is to fix it. Its test passes while the case still fails, and fails
 *  once the case passes: then take it off the list.
 *
 */

#include <format>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StorySim.h"
#include "PokemonBDSP_AutoStory_SimFailureCases_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::Pose;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;

//  Cases the program fails today, with what is to fix each.
const std::map<std::string, std::string> KNOWN_FAILING{
    {"01.05 old walk slides onto the Gate's west mouth, dim fade, banner gone", "#268 slice 2: walkers report warps"},
    {"01.05 old walk slides onto the Gate's west mouth, black fade", "#268 slice 2: walkers report warps"},
    {"01.05 old walk slides onto the Gate's west mouth, white fade", "#268 slices 2 and 3: a fade watcher that tells white from a battle"},
    {"01.05 through_the_gate is one run west", "planner: goal kinds and ms costs (PLANNER_DESIGN.md)"},
    {"any one missed fade or banner from 00.00 to 01.11: every row still passes", "#268 slice 1: door arrival = fade or banner"},
    {"01.08 with Route 204's youngster wandering (12..14, 2)", "planner: one recovery rule, a sidestep round a wanderer (PLANNER_DESIGN.md 4, 7)"},
};

//  The table's step `name` in row `row`.
const StoryStep& table_step(const std::string& row, const std::string& name){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
            if (checkpoint.id == row && step.name == name){
                return step;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No step " + name + " in " + row);
}

//  Runs rows `from` to `to` of `rows` with `misses` and `knobs`: "" if they
//  all pass, else where and why the run stopped.
std::string run_case(
    Logger& logger, const std::string& from, const std::string& to,
    SimMisses misses, SimWorldKnobs knobs, const std::vector<StoryCheckpoint>& rows = story_checkpoints()
){
    SimExecutor executor(logger, std::move(misses), std::move(knobs));
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = from, .to = to}, rows);
    return report.completed ? "" : std::format("stopped in {}: {}", executor.result().failed_step, executor.result().error);
}
UnitTestResult passes(const std::string& stopped){
    return stopped.empty() ? UnitTestResult(true) : UnitTestResult(stopped);
}

//  The old 01.05 walk west along row 22 to the tile beside the west mouth's
//  warp (3, 22), as before #269 stopped routes two short of warps.
StoryStep old_walk_beside_the_west_mouth(){
    StoryStep step = table_step("01.05", "through_the_gate");
    step.target = Pokemon::tile_pose("OreburghGate", {TilePos{4, 22}}, Direction::WEST);
    return step;
}

//  01.05's walk west through the Gate whose last open leg runs a tile on;
//  the old walk's slides onto the west mouth's warp to Route 203.
SimWorldKnobs gate_slide(SimFade fade, bool banner_up){
    const Pose far = *table_step("01.05", "out_of_oreburgh_gate").pose_after;
    return {.slides = {{.step = "through_the_gate", .direction = Direction::WEST, .tiles = 1,
        .far = far, .fade = fade, .banner_up = banner_up}}};
}
UnitTestResult gate_old_walk_slides(Logger& logger, SimFade fade, bool banner_up){
    const std::vector<StoryCheckpoint> rows = table_with("01.05", old_walk_beside_the_west_mouth());
    return passes(run_case(logger, "01.05", "01.05", {.fade = {}, .banner = {}}, gate_slide(fade, banner_up), rows));
}

//  The planner's legs for 01.05's walk through the Gate: one run west
//  along row 22 works on the console; at most a hold to find the row first.
UnitTestResult gate_plan_one_run(Logger& logger){
    const StoryWorld* world = story_plan_world("OreburghGate", "OreburghGate");
    const std::vector<TilePos> from{story_to_world(*world, "OreburghGate", {27, 22}), story_to_world(*world, "OreburghGate", {26, 22})};
    const TilePos goal = story_to_world(*world, "OreburghGate", {5, 22});
    const auto plan = plan_recovered_route(*world, from, [&](const TilePos& pos){ return pos == goal; }, *story_route_costs(table_step("01.05", "through_the_gate").variant));
    if (!plan){
        return "no plan";
    }
    std::string legs;
    for (const PlannedLeg& leg : *plan){
        legs += (legs.empty() ? "" : ", ") + to_string(leg.leg);
    }
    logger.log("through_the_gate plans: " + legs);
    return plan->size() <= 2 ? UnitTestResult(true) : UnitTestResult(std::format("{} legs: {}", plan->size(), legs));
}

//  Each fade and each banner the clean run looks for, missed alone: the
//  row it falls in must still pass. A door step accepts only its fade
//  today, so a missed fade there fails a real arrival (#268's Monte Carlo,
//  19/20 on 01.05).
UnitTestResult every_single_miss(Logger& logger){
    SimExecutor clean(logger);
    if (!run_rows_on(clean, logger, {.from = "00.00", .to = "01.11"}).completed){
        return "the run fails without misses: " + clean.result().error;
    }
    const std::vector<SimRun::RowStart>& rows = clean.result().rows;
    std::vector<std::string> failed;
    const auto miss = [&](bool fade, size_t index){
        size_t k = 0;
        while (k + 1 < rows.size() && (fade ? rows[k + 1].fades : rows[k + 1].banners) <= index){
            k++;
        }
        SimMisses misses{.fade = {}, .banner = {}};
        (fade ? misses.fade : misses.banner) = index - (fade ? rows[k].fades : rows[k].banners);
        const std::string stopped = run_case(logger, rows[k].row, rows[k].row, misses, {});
        if (!stopped.empty()){
            failed.push_back(std::format("{} {} missed: {}", fade ? "fade" : "banner", index, stopped));
        }
    };
    for (size_t i = 0; i < clean.misses().fades; i++){
        miss(true, i);
    }
    for (size_t i = 0; i < clean.misses().banners; i++){
        miss(false, i);
    }
    std::string out = std::format("{} of {} single misses fail their row:", failed.size(), clean.misses().fades + clean.misses().banners);
    for (const std::string& line : failed){
        out += "\n  " + line;
    }
    return failed.empty() ? UnitTestResult(true) : UnitTestResult(out);
}

//  Oreburgh's street at night (88, game clock 00:25; 104 by day, r74), the
//  Gate's dim floor (46) and a cave mouth's dim fade (45 to 64): the tap
//  steps judge their fades with tap_faded(), which took the Gate's floor
//  for a fade under the old "brightness under 75" (lab-B 2026-11-08).
UnitTestResult night_and_dim_floors(Logger& logger){
    SimWorldKnobs knobs{
        .floor = {{"OreburghCity", 88}, {"Route203", 88}, {"OreburghGate", 46}},
        .fade_into = {{"OreburghGate", 64}, {"OreburghCity", 255}, {"Route203", 255}},
    };
    return passes(run_case(logger, "01.02", "01.05", {.fade = {}, .banner = {}}, knobs));
}

//  01.03: Roark's first line in the mine comes after the camera zooms in on
//  us, some seconds after the A press (r98; the old event waited 8 s).
UnitTestResult roark_speaks_late(Logger& logger){
    std::string talk;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (row.id == "01.03" && step.kind == StoryStepKind::TALK){
                talk = step.name;
            }
        }
    }
    return passes(run_case(logger, "01.03", "01.03", {.fade = {}, .banner = {}}, {.first_line = {{talk, 7000ms}}}));
}

//  `row` with everyone on its maps walking their wander boxes, seeds 1 to 8.
UnitTestResult wanderers_move(Logger& logger, const std::string& row){
    std::vector<std::string> failed;
    for (uint64_t seed = 1; seed <= 8; seed++){
        const std::string stopped = run_case(logger, row, row, {.fade = {}, .banner = {}}, {.wander_seed = seed});
        if (!stopped.empty()){
            failed.push_back(std::format("seed {}: {}", seed, stopped));
        }
    }
    std::string out = std::format("{} of 8 seeds fail:", failed.size());
    for (const std::string& line : failed){
        out += "\n  " + line;
    }
    return failed.empty() ? UnitTestResult(true) : UnitTestResult(out);
}

struct FailureCaseInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};
//  Whether a case is expected to fail is KNOWN_FAILING's, by name.
struct FailureCaseExpectation{
};
struct FailureCase{
    FailureCaseInput input;
    FailureCaseExpectation expectation;
};

class Test_AutoStoryFailureCase : public UnitTest{
public:
    explicit Test_AutoStoryFailureCase(FailureCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Console failure on the sim: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        UnitTestResult result;
        try{
            result = m_case.input.body(logger);
        }catch (Exception& e){
            result = std::string(e.name()) + ": " + e.message();
        }
        const auto known = KNOWN_FAILING.find(m_case.input.name);
        if (known == KNOWN_FAILING.end()){
            return result;
        }
        if (result.result == UnitTestResult::PASSED){
            return "on KNOWN_FAILING (" + known->second + ") but passes now: take it off the list.";
        }
        logger.log("Known failing until " + known->second + ": " + result.message);
        return true;
    }

private:
    FailureCase m_case;
};

} // namespace

const FailureCase AUTO_STORY_FAILURE_CASES[] = {
    {.input = {"01.05 through_the_gate's last leg runs a tile on (lab-B 2026-11-08 15:31)", [](Logger& logger){
        return passes(run_case(logger, "01.05", "01.05", {.fade = {}, .banner = {}}, gate_slide(SimFade::DIM, false)));
    }}, .expectation = {}},
    {.input = {"01.05 old walk slides onto the Gate's west mouth, dim fade, banner gone", [](Logger& logger){
        return gate_old_walk_slides(logger, SimFade::DIM, false);
    }}, .expectation = {}},
    {.input = {"01.05 old walk slides onto the Gate's west mouth, dim fade, banner up", [](Logger& logger){
        return gate_old_walk_slides(logger, SimFade::DIM, true);
    }}, .expectation = {}},
    {.input = {"01.05 old walk slides onto the Gate's west mouth, black fade", [](Logger& logger){
        return gate_old_walk_slides(logger, SimFade::BLACK, true);
    }}, .expectation = {}},
    {.input = {"01.05 old walk slides onto the Gate's west mouth, white fade", [](Logger& logger){
        return gate_old_walk_slides(logger, SimFade::WHITE, true);
    }}, .expectation = {}},
    {.input = {"01.05 through_the_gate is one run west", gate_plan_one_run}, .expectation = {}},
    {.input = {"any one missed fade or banner from 00.00 to 01.11: every row still passes", every_single_miss}, .expectation = {}},
    {.input = {"01.02 to 01.05 at night, the Gate's dim floor", night_and_dim_floors}, .expectation = {}},
    {.input = {"01.03 Roark's first line 7 s after the A press", roark_speaks_late}, .expectation = {}},
    {.input = {"01.03 with Oreburgh's people wandering (the pokefan on 45, 37..39)", [](Logger& logger){
        return wanderers_move(logger, "01.03");
    }}, .expectation = {}},
    {.input = {"01.08 with Route 204's youngster wandering (12..14, 2)", [](Logger& logger){
        return wanderers_move(logger, "01.08");
    }}, .expectation = {}},
};

void add_tests_AutoStory_SimFailureCases(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_FAILURE_CASES){
        database.add<Test_AutoStoryFailureCase>(item);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
