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
#include <optional>
#include <set>
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
    {"01.05 through_the_gate is one run west", "planner: goal kinds and ms costs (PLANNER_DESIGN.md)"},
    {"any one missed fade or banner from 00.00 to 01.11: every row still passes",
        "a third signal where a warp gives one (ONE_SIGNAL_STEPS): position (PLANNER_DESIGN.md 2, 4)"},
    {"00.03 out of the house from each tile Rowan's scene ends on, rows 8 and 9 (lab-B 2026-11-09)",
        "00.03: the way to the door localized from each tile the scene ends on"},
    {"01.08 with Route 204's youngster wandering (12..14, 2)", "planner: one recovery rule, a sidestep round a wanderer (PLANNER_DESIGN.md 4, 7)"},
};

//  Warps that give one signal only (#268 slice 1: door = fade or banner), so
//  a miss there leaves nothing to go on: a fade and no banner (stairs, into
//  a building, between the mine's floors), or a banner and no fade (an open
//  border crossed by run_until_map()).
const std::set<std::string> ONE_SIGNAL_STEPS{
    "00.01 down_the_stairs", "00.01 into_barrys_house", "00.01 up_barrys_stairs", "00.01 down_barrys_stairs",
    "00.04 into_the_house", "00.05 into_the_school", "01.03 into_the_coal_room", "01.03 up_to_the_hall",
    "00.04 to_route_201", "01.10 into_floaroma",
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

//  What a single miss leaves out: a fade whole, a fade's hold, or a banner.
enum class SingleMiss{
    FADE,
    HOLD,
    BANNER,
};

//  The run of the row the clean run's `index`th fade or banner (`rows`'
//  counts) falls in, with that one missed as `kind` says; none if it passes.
std::optional<SimRun> run_single_miss(Logger& logger, const std::vector<SimRun::RowStart>& rows, SingleMiss kind, size_t index){
    const bool fade = kind != SingleMiss::BANNER;
    size_t k = 0;
    while (k + 1 < rows.size() && (fade ? rows[k + 1].fades : rows[k + 1].banners) <= index){
        k++;
    }
    const size_t in_row = index - (fade ? rows[k].fades : rows[k].banners);
    SimMisses misses{.fade = {}, .banner = {}};
    (kind == SingleMiss::FADE ? misses.fade : kind == SingleMiss::HOLD ? misses.hold : misses.banner) = in_row;
    SimExecutor executor(logger, misses);
    if (run_rows_on(executor, logger, {.from = rows[k].row, .to = rows[k].row}).completed){
        return std::nullopt;
    }
    return executor.result();
}

//  Each fade and each banner the clean run looks for, missed alone: the
//  row it falls in must still pass, but for a miss in one of `skip`'s steps.
//  A door step once accepted only its fade, so a missed fade there failed a
//  real arrival (#268's Monte Carlo, 19/20 on 01.05).
UnitTestResult every_single_miss(Logger& logger, const std::set<std::string>& skip){
    SimExecutor clean(logger);
    if (!run_rows_on(clean, logger, {.from = "00.00", .to = "01.11"}).completed){
        return "the run fails without misses: " + clean.result().error;
    }
    const std::vector<SimRun::RowStart>& rows = clean.result().rows;
    std::vector<std::string> failed;
    const auto miss = [&](bool fade, size_t index){
        const std::optional<SimRun> run = run_single_miss(logger, rows, fade ? SingleMiss::FADE : SingleMiss::BANNER, index);
        if (run && !skip.contains(run->failed_step)){
            failed.push_back(std::format("{} {} missed: stopped in {}: {}", fade ? "fade" : "banner", index,
                run->failed_step, run->error));
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

//  Each fade the clean run looks for with only its hold unseen, a short
//  fade between two frames (the door's watcher missed 13 of 35 fades on
//  lab-B, 2026-11-08): the program's fade watch takes the scene after the
//  fall, so every row passes, at every warp, ONE_SIGNAL_STEPS' too.
UnitTestResult every_single_hold_miss(Logger& logger){
    SimExecutor clean(logger);
    if (!run_rows_on(clean, logger, {.from = "00.00", .to = "01.11"}).completed){
        return "the run fails without misses: " + clean.result().error;
    }
    std::vector<std::string> failed;
    for (size_t i = 0; i < clean.misses().fades; i++){
        const std::optional<SimRun> run = run_single_miss(logger, clean.result().rows, SingleMiss::HOLD, i);
        if (run){
            failed.push_back(std::format("fade {}'s hold unseen: stopped in {}: {}", i, run->failed_step, run->error));
        }
    }
    std::string out = std::format("{} of {} fades with their hold unseen fail their row:", failed.size(), clean.misses().fades);
    for (const std::string& line : failed){
        out += "\n  " + line;
    }
    return failed.empty() ? UnitTestResult(true) : UnitTestResult(out);
}

//  Oreburgh's street at night (88, game clock 00:25; 104 by day, r74), the
//  Gate's dim floor (46) and a cave mouth's dim fade (45 to 64): the tap
//  steps judge their fades with fade_of(): uniform, so a textured floor
//  is none. The old "brightness under 75" took the Gate's floor for a fade
//  (lab-B 2026-11-08).
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

//  Roark's badge speech after the battle: four lines, the fanfare's pause
//  with no box, then the badge's and the TM's lines (lab-B 2026-10-31 09:45
//  and 2026-11-08 15:06: "no box for a moment" read as the end, and the
//  Potion's menu opened under his next line). Then a scene: someone walks
//  off with no box for 6 s before the last line, as Rowan does in r23 (the
//  walk is a model, not measured after Roark). Played on the talk-over the
//  battle's end runs (confirm_battle_overworld()).
UnitTestResult roark_badge_speech(Logger& logger){
    const SimScene scene{
        sim_line("Roark: the battle's last line"),
        sim_line("Roark: you beat me"),
        sim_line("Roark: the Coal Badge"),
        sim_line("Received the Coal Badge"),
        sim_pause("the badge's fanfare", 3000ms, false),
        sim_line("Roark: the TM"),
        sim_line("Roark: Stealth Rock"),
        sim_pause("someone walks off", 6000ms, true),
        sim_line("the last line"),
    };
    const std::string early = play_scene(scene, {});
    logger.log("Roark's badge speech: " + (early.empty() ? std::string("over after the last line") : early));
    return passes(early);
}

//  01.07: after the tag battle the screen fades back from the battle and
//  the grunts run off before anyone speaks: a scene that opens with no box,
//  6 s of people moving (Rowan's cutscene shape), then Rowan's and Dawn's
//  lines, and Bebe walks over with hers. The lengths are a model: r80 saved
//  over her lines.
UnitTestResult grunts_run_off_before_rowan(Logger& logger){
    SimWorldKnobs knobs;
    knobs.scenes["galactic_grunts"] = {
        sim_fade("back from the battle", 1000ms),
        sim_pause("the grunts run off", 6000ms, true),
        sim_line("Rowan"),
        sim_line("Rowan"),
        sim_line("Dawn"),
        sim_pause("Bebe walks over", 5000ms, true),
        sim_line("Bebe"),
        sim_line("Bebe: the Pokemon Center's PC"),
    };
    return passes(run_case(logger, "01.07", "01.07", {.fade = {}, .banner = {}}, knobs));
}

//  00.03: Rowan's scene opens with no box: Rowan and Dawn walk back and
//  stand a moment before his first line; after he leaves, Dawn's and
//  Barry's lines, the fade home, and Mom walks over before she speaks
//  (r23, r24, r31). The scene changes map, so only the quiet after the
//  fade counts.
UnitTestResult rowans_scene_opens_with_no_box(Logger& logger){
    SimWorldKnobs knobs;
    knobs.scenes["rowan_and_dawn"] = {
        sim_pause("Rowan and Dawn walk back", 7000ms, true),
        sim_pause("they stand a moment", 3000ms, false),
        sim_line("Rowan"),
        sim_line("Rowan"),
        sim_pause("Rowan leaves", 6000ms, true),
        sim_pause("Dawn and Barry stand", 3000ms, false),
        sim_line("Dawn"),
        sim_line("Barry"),
        sim_fade("the fade home", 1500ms),
        sim_pause("home, still", 4000ms, false),
        sim_pause("Mom walks over", 5000ms, true),
        sim_line("Mom: the Running Shoes"),
        sim_line("Mom"),
    };
    return passes(run_case(logger, "00.03", "00.03", {.fade = {}, .banner = {}}, knobs));
}

//  00.02: the Starly battle's opening is still playing when the battle
//  step starts, right after choose_starter (lab-B 2026-11-09 run 3, event
//  7): the battle step's, not interrupt_unsettled before it.
UnitTestResult starly_opening_after_the_starter(Logger& logger){
    SimWorldKnobs knobs;
    knobs.battle_opening_before = [](const std::string& row, const std::string& step){
        return row == "00.02" && step == "starly";
    };
    return passes(run_case(logger, "00.02", "00.02", {.fade = {}, .banner = {}}, knobs));
}

//  00.03: Rowan's scene ends at home beside Mom, on the rug's row (r24,
//  r27, r28) or a row lower (lab-B 2026-11-09 12:49: the walk west to the
//  table's corner stopped on (5, 9) against the wall (4, 9), and the taps
//  after crossed the door's mat to (7, 10)). The way out goes from each.
UnitTestResult home_from_each_scene_end(Logger& logger){
    std::string failures;
    for (const TilePos& tile : {TilePos{5, 8}, TilePos{6, 8}, TilePos{5, 9}, TilePos{6, 9}}){
        SimWorldKnobs knobs;
        knobs.lands["rowan_and_dawn"] = Pokemon::tile_pose("PlayerHouse1F", {tile}, Direction::EAST);
        const std::string stopped = run_case(logger, "00.03", "00.03", {.fade = {}, .banner = {}}, std::move(knobs));
        if (!stopped.empty()){
            failures += std::format("\nfrom ({}, {}): {}", tile.x, tile.y, stopped);
        }
    }
    return passes(failures);
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
    {.input = {"any one missed fade or banner from 00.00 to 01.11: every row still passes", [](Logger& logger){
        return every_single_miss(logger, {});
    }}, .expectation = {}},
    {.input = {"any one missed fade or banner at a warp that shows both: its row still passes", [](Logger& logger){
        return every_single_miss(logger, ONE_SIGNAL_STEPS);
    }}, .expectation = {}},
    {.input = {"any one fade with its hold unseen from 00.00 to 01.11: every row still passes", every_single_hold_miss}, .expectation = {}},
    {.input = {"01.02 to 01.05 at night, the Gate's dim floor", night_and_dim_floors}, .expectation = {}},
    {.input = {"01.03 Roark's first line 7 s after the A press", roark_speaks_late}, .expectation = {}},
    {.input = {"01.03 with Oreburgh's people wandering (the pokefan on 45, 37..39)", [](Logger& logger){
        return wanderers_move(logger, "01.03");
    }}, .expectation = {}},
    {.input = {"01.08 with Route 204's youngster wandering (12..14, 2)", [](Logger& logger){
        return wanderers_move(logger, "01.08");
    }}, .expectation = {}},
    {.input = {"01.04 Roark's badge speech: the fanfare's pause, then a scene", roark_badge_speech}, .expectation = {}},
    {.input = {"01.07 after the tag battle the grunts run off before anyone speaks", grunts_run_off_before_rowan}, .expectation = {}},
    {.input = {"00.03 Rowan's scene opens with no box and fades home", rowans_scene_opens_with_no_box}, .expectation = {}},
    {.input = {"00.02 the Starly battle's opening still plays after the starter (lab-B 2026-11-09)", starly_opening_after_the_starter}, .expectation = {}},
    {.input = {"00.03 out of the house from each tile Rowan's scene ends on, rows 8 and 9 (lab-B 2026-11-09)", home_from_each_scene_end}, .expectation = {}},
};

void add_tests_AutoStory_SimFailureCases(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_FAILURE_CASES){
        database.add<Test_AutoStoryFailureCase>(item);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
