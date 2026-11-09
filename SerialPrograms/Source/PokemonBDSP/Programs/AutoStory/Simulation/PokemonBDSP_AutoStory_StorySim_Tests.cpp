/*  BDSP AutoStory Story Sim Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows of the story table run as the program runs them (the segment
 *  runner, make_story_segment()), with the program's own walking steps
 *  (StoryWalkSteps: ROUTE, LEGS, STEP, TALK, MASH) on the grid simulation.
 *  After each walking step the simulated player must stand, on the grid,
 *  on a tile the table gives; a step through a door must have warped.
 *
 *  Events, battles, shops and saves are stubs: a dialogue they open is
 *  closed, and the table's pose_after puts the player where they end. A
 *  heal checks that a walk north from where we stand enters a door. A scene
 *  starts when the player steps onto one of the maps' coord events while
 *  its story variable holds (story_vars()). Rooms with no
 *  map (the Trainers' School) are passed over. Each row must start where
 *  the row before left the player.
 *
 *  A tap or hold through a door or cave mouth stops on its tile, as the
 *  console stops at the fade; a hold that does not see the fade goes on
 *  onto the step's pose_after and on along it, and a location banner reads
 *  only a few tiles into its map (SIM_BANNER_TILES). The exit tests run
 *  r75's and r81's old open-ended walks that way.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_CoordEvents.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_OldStorySteps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_StorySim.h"
#include "PokemonBDSP_AutoStory_StorySim_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;

UnitTestResult run_exit_test(Logger& logger, const std::string& row_id, const StoryStep& old_step, const SimMisses& misses, const Pose& overshoot){
    SimExecutor executor(logger, misses);
    const std::vector<StoryCheckpoint> rows = table_with(row_id, old_step);
    run_rows_on(executor, logger, {.from = row_id, .to = row_id}, rows);
    const SimRun& run = executor.result();
    logger.log(std::format("The old {}: {}, ended on {}.", old_step.name, run.error, end_of(run)));
    if (run.failed_step != row_id + " " + old_step.name || !ends_on(run, overshoot)){
        return std::format("the old {} should end on {} and fail there; it ended on {}: {}",
            old_step.name, Pokemon::to_string(overshoot), end_of(run), run.error.empty() ? "no failure" : run.error);
    }
    SimExecutor clean(logger);
    if (!run_rows_on(clean, logger, {.from = row_id, .to = row_id}).completed){
        return "the table's row fails without misses: " + clean.result().error;
    }
    std::vector<SimMisses> runs;
    for (size_t i = 0; i < clean.misses().fades; i++){
        runs.push_back({.fade = i, .banner = std::nullopt});
    }
    for (size_t i = 0; i < clean.misses().banners; i++){
        runs.push_back({.fade = std::nullopt, .banner = i});
    }
    for (const SimMisses& m : runs){
        SimExecutor sim(logger, m);
        run_rows_on(sim, logger, {.from = row_id, .to = row_id});
        if (ends_on(sim.result(), overshoot)){
            return std::format("the table's {} ended on {} with a miss in {}.", old_step.name, end_of(sim.result()),
                sim.result().missed_step);
        }
    }
    return true;
}

UnitTestResult test_coord_events(Logger&){
    for (const auto& [map, json] : bdsp_map_json()){
        const StoryWorld* world = story_plan_world(map, map);
        for (const CoordEvent& event : world == nullptr ? std::vector<CoordEvent>{} : story_coord_events(map)){
            const auto open = [&](const TilePos& pos){
                const TilePos tile = story_to_world(*world, map, pos);
                return standable(*world, tile) || world->tile(tile) == Pokemon::TileKind::DOOR;
            };
            if (!std::ranges::any_of(event.tiles, open)){
                return std::format("{} {}: no tile is open ground or a door.", map, event.name);
            }
        }
    }
    const size_t armed = story_triggers().size();
    return armed == story_vars().size() ? UnitTestResult(true)
        : UnitTestResult(std::format("Expected {} coord events armed by the story's variables, found {}.", story_vars().size(), armed));
}

std::string miss(Logger& logger, const SimRun& clean, bool fade, size_t index){
    size_t k = 0;
    while (k + 1 < clean.rows.size() && (fade ? clean.rows[k + 1].fades : clean.rows[k + 1].banners) <= index){
        k++;
    }
    const SimRun::RowStart& start = clean.rows[k];
    SimMisses misses;
    (fade ? misses.fade : misses.banner) = index - (fade ? start.fades : start.banners);
    SimExecutor executor(logger, misses);
    const Pokemon::SegmentReport report = run_rows_on(executor, logger,
        {.from = start.row, .to = clean.rows[std::min(k + 1, clean.rows.size() - 1)].row});
    const SimRun& run = executor.result();
    const std::string what = std::format("{} {}", fade ? "fade" : "banner", index);
    logger.log(std::format("Missed {} in {}: {}.", what, run.missed_step,
        report.completed ? "got over it" : "stopped in " + run.failed_step + ": " + run.error));
    if (report.completed || run.failed_step == run.missed_step){
        return "";
    }
    return std::format("{} missed in {}, failed later in {}: {}", what, run.missed_step, run.failed_step, run.error);
}

UnitTestResult test_story_sim_misses(Logger& logger){
    Pokemon::SegmentRange range{.from = "00.00", .to = "01.11"};
    SimExecutor clean(logger);
    if (!run_rows_on(clean, logger, range).completed){
        return "the run fails without misses: " + clean.result().error;
    }
    logger.log(std::format("{} fades and {} banners to miss.", clean.misses().fades, clean.misses().banners));
    std::vector<std::string> unseen;
    for (size_t i = 0; i < clean.misses().fades + clean.misses().banners; i++){
        const std::string line = miss(logger, clean.result(), i < clean.misses().fades, i < clean.misses().fades ? i : i - clean.misses().fades);
        if (!line.empty()){
            unseen.push_back(line);
        }
    }
    std::string out = "Misses that went unseen:";
    for (const std::string& line : unseen){
        out += "\n  " + line;
    }
    return unseen.empty() ? UnitTestResult(true) : UnitTestResult(out);
}

UnitTestResult test_story_sim_whole_table(Logger& logger){
    return run_rows(logger, {.from = "00.00", .to = "01.11"});
}

UnitTestResult test_story_sim_each_row(Logger& logger){
    for (const StoryCheckpoint& row : story_checkpoints()){
        const auto res = run_rows(logger, {.from = row.id, .to = row.id});
        if (res.result != UnitTestResult::PASSED){
            return std::format("Row {} failed: {}", row.id, res.message);
        }
    }
    return true;
}

UnitTestResult test_exit_gate(Logger& logger){
    return run_exit_test(logger, "01.05", r75_old_gate_exit(), SimMisses{}, r75_pond_pocket());
}

UnitTestResult test_exit_ravaged_path(Logger& logger){
    return run_exit_test(logger, "01.09", r81_old_cave_exit(), SimMisses{.fade = 0, .banner = std::nullopt}, r81_below_the_mouth());
}

//  lab-B 2026-11-08 15:31 to 15:54: 01.05's walk to the tile beside the
//  Oreburgh Gate's mouth slid onto the warp, and the tap step after it began
//  inside the Gate. With its banner up, the step ends there with no tap and
//  no fade (the clean run's fade into the Gate is not seen); with it gone,
//  it fails a few taps on, not across the Gate.
UnitTestResult test_tap_through_already(Logger& logger, bool banner){
    const std::vector<StoryCheckpoint> rows = table_with("01.05", old_tap_into_the_gate());
    SimExecutor clean(logger);
    run_rows_on(clean, logger, {.from = "01.05", .to = "01.05"}, rows);
    SimExecutor through(logger, {.fade = {}, .banner = {}, .through = 1, .through_banner = banner});
    run_rows_on(through, logger, {.from = "01.05", .to = "01.05"}, rows);
    const SimRun& run = through.result();
    const std::optional<TilePos> at = run.world ? story_to_local(*run.world, "OreburghGate", run.end) : std::nullopt;
    const bool passed = banner
        ? run.error.empty() && through.misses().fades + 1 == clean.misses().fades
        : run.failed_step == "01.05 into_oreburgh_gate" && at && at->y == 22 && at->x >= 21;
    return passed ? UnitTestResult(true) : UnitTestResult(std::format("failed in '{}' on {} after {} fades (clean {}): {}",
        run.failed_step, end_of(run), through.misses().fades, clean.misses().fades, run.error.empty() ? "no failure" : run.error));
}

UnitTestResult test_story_sim_step_failure(Logger& logger, const std::string& row, const StoryStep& old_step, const std::string& expected_failed_step, const std::string& expected_error){
    SimExecutor executor(logger);
    const std::vector<StoryCheckpoint> rows = table_with(row, old_step);
    run_rows_on(executor, logger, {.from = row, .to = row}, rows);
    const SimRun& run = executor.result();
    if (run.failed_step != expected_failed_step || run.error.find(expected_error) == std::string::npos){
        return std::format("expected old {} to fail at '{}' with '{}', but got failed_step='{}', error='{}'.",
            old_step.name, expected_failed_step, expected_error, run.failed_step, run.error);
    }
    return true;
}

//  lab-B 2026-11-08: 01.03's run into Oreburgh's Center saw no fade and went
//  on to the nurse's counter. The heal must find it is inside and go on.
UnitTestResult test_heal_fade_unseen(Logger& logger){
    SimExecutor clean(logger);
    run_rows_on(clean, logger, {.from = "01.03", .to = "01.03"});
    for (size_t i = 0; i < clean.misses().fades; i++){
        SimExecutor sim(logger, {.fade = i, .banner = {}});
        run_rows_on(sim, logger, {.from = "01.03", .to = "01.03"});
        const SimRun& run = sim.result();
        if (run.missed_step == "01.03 heal_oreburgh"){
            return run.error.empty() ? UnitTestResult(true) : UnitTestResult(std::format("failed in '{}': {}", run.failed_step, run.error));
        }
    }
    return "no fade in 01.03 heal_oreburgh to miss: the heal never ran into its door.";
}

struct AutoStoryStorySimInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryStorySimExpectation{
};

struct AutoStoryStorySimCase{
    AutoStoryStorySimInput input;
    AutoStoryStorySimExpectation expectation;
};

class Test_AutoStoryStorySim : public UnitTest{
public:
    explicit Test_AutoStoryStorySim(AutoStoryStorySimCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Story sim: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryStorySimCase m_case;
};

} // namespace

const AutoStoryStorySimCase AUTO_STORY_STORY_SIM_CASES[] = {
    {.input = {"the maps' coord events are on open ground or doors", test_coord_events}, .expectation = {}},
    {.input = {"detector misses from 00.00 to 01.11", test_story_sim_misses}, .expectation = {}},
    {.input = {"00.00 to 01.11: the whole table, from the new game to Floaroma", test_story_sim_whole_table}, .expectation = {}},
    {.input = {"each row from 00.00 to 01.11 in isolation", test_story_sim_each_row}, .expectation = {}},
    {.input = {"01.02b, Barry's lines, then the Center past his doorstep (lab-B, 2026-11-08)", [](Logger& logger){
        return run_rows(logger, {.from = "01.02b", .to = "01.02b"});
    }}, .expectation = {}},
    {.input = {"r75, out of the Oreburgh Gate", test_exit_gate}, .expectation = {}},
    {.input = {"r81, out of the Ravaged Path", test_exit_ravaged_path}, .expectation = {}},
    {.input = {"01.05, a tap step begun through the Gate's mouth, its banner up (lab-B, 2026-11-08)", [](Logger& logger){
        return test_tap_through_already(logger, true);
    }}, .expectation = {}},
    {.input = {"01.05, a tap step begun through the Gate's mouth, its banner gone (lab-B, 2026-11-08)", [](Logger& logger){
        return test_tap_through_already(logger, false);
    }}, .expectation = {}},
    {
        .input = {"01.03, a dark frame 1.8 s down the mine's entrance hall is not its mouth (lab-B, 2026-11-08)", [](Logger& logger){
            SimExecutor executor(logger, {.fade = {}, .banner = {}, .dark = 1});
            run_rows_on(executor, logger, {.from = "01.03", .to = "01.03"});
            const SimRun& run = executor.result();
            return run.missed_step == "01.03 into_the_coal_room" && run.error.empty() ? UnitTestResult(true)
                : UnitTestResult(std::format("dark in '{}', failed in '{}': {}", run.missed_step, run.failed_step, run.error));
        }},
        .expectation = {},
    },
    {
        .input = {"01.03, the Center's fade unseen, the heal goes on from the counter (lab-B, 2026-11-08)", test_heal_fade_unseen},
        .expectation = {},
    },
    {
        .input = {"01.03, counted 19 taps south leaves player in hall", [](Logger& logger){
            return test_story_sim_step_failure(logger, "01.03", old_mine_hall_19("into_the_coal_room", Direction::SOUTH), "01.03 south_to_the_rock", "warp instead of a stop.");
        }},
        .expectation = {},
    },
    {
        .input = {"01.03, counted 19 taps north leaves player in hall", [](Logger& logger){
            return test_story_sim_step_failure(logger, "01.03", old_mine_hall_19("out_of_the_mine", Direction::NORTH), "01.03 north_to_the_centers_wall", "map_mismatch");
        }},
        .expectation = {},
    },
    {
        .input = {"01.03, cut corner through coal room crate at col 29", [](Logger& logger){
            return test_story_sim_step_failure(logger, "01.03", old_coal_room_crate_cut(), "01.03 to_roarks_corner", "short_leg: south 2 moved 1");
        }},
        .expectation = {},
    },
};

void add_tests_AutoStory_StorySim(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_STORY_SIM_CASES){
        database.add<Test_AutoStoryStorySim>(item);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
