/*  BDSP AutoStory - Console failure cases on the story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <cstdio>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/Navigation/Simulation/Pokemon_KnownFailing.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimSeeds.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStoryCenter.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryRobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryRoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryWallRecovery.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryPlaces.h"
#include "PokemonBDSP_AutoStoryLegacySteps.h"
#include "PokemonBDSP_AutoStorySim.h"
#include "PokemonBDSP_AutoStorySimFailureCases_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace detail;

namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::TilePos;

const Pokemon::KnownFailing& known_failing(){
    static const Pokemon::KnownFailing list({
        {"01.05 through_the_gate is one run west", "planner: goal kinds and ms costs (PLANNER_DESIGN.md)"},
        {"any one missed fade or banner from 00.00 to 01.11: every row still passes",
            "a third signal where a warp gives one (ONE_SIGNAL_STEPS): position (PLANNER_DESIGN.md 2, 4)"},
    });
    return list;
}

const std::set<std::string> ONE_SIGNAL_STEPS{
    "00.01 down_the_stairs", "00.01 into_barrys_house", "00.01 up_barrys_stairs", "00.01 down_barrys_stairs",
    "00.04 into_the_house", "00.05 into_the_school", "01.03 into_the_coal_room", "01.03 up_to_the_hall",
    "00.04 to_route_201", "01.10 into_floaroma",
};

const BdspStep& table_step(std::string_view row, std::string_view name){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const BdspStep& step : checkpoint.steps){
            if (checkpoint.id == row && step.name == name){
                return step;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No step " + std::string(name) + " in " + std::string(row));
}

std::string run_case(
    Logger& logger, const std::string& from, const std::string& to,
    SimMisses misses, SimWorldKnobs knobs, const std::vector<StoryCheckpoint>& rows = story_checkpoints()
){
    SimExecutor executor(logger, std::move(misses), std::move(knobs));
    const SimRunReport report = run_rows_on(executor, logger, {.from = from, .to = to}, rows);
    return report.completed ? "" : std::format("stopped in {}: {}", executor.result().failed_step, executor.result().error);
}
UnitTestResult passes(const std::string& stopped){
    return stopped.empty() ? UnitTestResult(true) : UnitTestResult(stopped);
}

BdspStep old_walk_beside_the_west_mouth(){
    BdspStep step = table_step("01.05", "through_the_gate");
    std::get<RouteStepArgs>(step.args).target = GridPose{.map = "OreburghGate", .tiles = {{4, 22}}, .facing = Direction::WEST};
    return step;
}

SimWorldKnobs gate_slide(SimFade fade, bool banner_up){
    const GridPose far = *table_step("01.05", "out_of_oreburgh_gate").pose_after;
    return {.slides = {{.step = "through_the_gate", .direction = Direction::WEST, .tiles = 1,
        .far = far, .fade = fade, .banner_up = banner_up}}};
}
UnitTestResult gate_old_walk_slides(Logger& logger, SimFade fade, bool banner_up){
    const std::vector<StoryCheckpoint> rows = table_with("01.05", old_walk_beside_the_west_mouth());
    return passes(run_case(logger, "01.05", "01.05", {.fade = {}, .banner = {}}, gate_slide(fade, banner_up), rows));
}

UnitTestResult gate_plan_one_run(Logger& logger){
    const StoryWorld* world = story_plan_world("OreburghGate", "OreburghGate");
    const std::vector<TilePos> from{story_to_world(*world, "OreburghGate", {27, 22}), story_to_world(*world, "OreburghGate", {26, 22})};
    const TilePos goal = story_to_world(*world, "OreburghGate", {5, 22});
    const std::string& variant = std::get<RouteStepArgs>(table_step("01.05", "through_the_gate").args).variant;
    const auto plan = plan_recovered_route(*world, from, [&](const TilePos& pos){ return pos == goal; }, *story_route_costs(variant));
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

enum class SingleMiss{
    FADE,
    HOLD,
    BANNER,
};

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

UnitTestResult night_and_dim_floors(Logger& logger){
    SimWorldKnobs knobs{
        .floor = {{"OreburghCity", 88}, {"Route203", 88}, {"OreburghGate", 46}},
        .fade_into = {{"OreburghGate", 64}, {"OreburghCity", 255}, {"Route203", 255}},
    };
    return passes(run_case(logger, "01.02", "01.05", {.fade = {}, .banner = {}}, knobs));
}

UnitTestResult roark_speaks_late(Logger& logger){
    std::string talk;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const BdspStep& step : row.steps){
            if (row.id == "01.03" && step.action == "talk"){
                talk = step.name;
            }
        }
    }
    return passes(run_case(logger, "01.03", "01.03", {.fade = {}, .banner = {}}, {.first_line = {{talk, 7000ms}}}));
}

UnitTestResult mash_scene_speaks_late(Logger& logger){
    const Milliseconds delays[] = {7000ms, 9000ms};
    for (Milliseconds delay : delays){
        SimWorldKnobs knobs;
        knobs.first_line["barry_warns_us"] = delay;
        knobs.scenes["barry_warns_us"] = {sim_line("Barry warns the player")};
        const std::string stopped = run_case(logger, "00.01", "00.01", {.fade = {}, .banner = {}}, knobs);
        if (!stopped.empty()){
            return std::format("The scene with its first line at {} ms {}", delay.count(), stopped);
        }
    }
    return true;
}

constexpr uint32_t WANDER_SEEDS = 8;

UnitTestResult wanderers_move(Logger& logger, const std::string& row){
    const Pokemon::SeedsResult seeds = Pokemon::run_seeds(logger, std::format("{} wanderers", row), [&](uint32_t seed){
        const std::string stopped = run_case(logger, row, row, {.fade = {}, .banner = {}}, {.wander_seed = seed});
        return Pokemon::SeedRun{.failure = stopped.empty() ? std::nullopt : std::optional<std::string>(stopped)};
    }, WANDER_SEEDS);
    return Pokemon::sweep_result(logger, row, "wanderers", seeds.seeds, seeds.failures);
}

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

UnitTestResult center_inside_settles(Logger& logger){
    struct Inside{
        std::string_view what;
        Milliseconds fade;
        SimBeat after;
    };
    const Milliseconds most_after_fade = 3000ms;
    const Inside cases[] = {
        {"visitors walking about", 1500ms, sim_pause("visitors walking about", 60000ms, true)},
        {"a visitor walking past the player", 1500ms, sim_walk_past("a visitor walking past the player", 60000ms)},
        {"a slow fade-in", 4000ms, sim_pause("visitors walking about", 60000ms, true)},
    };
    for (const Inside& inside : cases){
        const SimScene scene{
            sim_fade("the Center's door fade", inside.fade),
            inside.after,
        };
        const std::string over = play_scene(scene, settle_rule(SettleKind::DOOR));
        logger.log(std::format("Center inside, {}: {}", inside.what, over));
        const long long least = inside.fade.count();
        const long long most = (inside.fade + most_after_fade).count();
        long long ms = -1;
        if (std::sscanf(over.c_str(), "judged over %lld ms in", &ms) != 1 || ms < least || ms > most){
            return std::format("Center inside, {}: not settled between {} and {} ms: {}", inside.what, least, most, over);
        }
    }
    return true;
}

UnitTestResult nurse_in_one_run(Logger&){
    const Milliseconds needed = ROUTE_START_DELAY + route_ms_per_tile(Direction::NORTH) * (int64_t)CENTER_MAT_TO_COUNTER_TILES;
    if (NURSE_RUN_HOLD < needed){
        return std::format("the run to the nurse holds {} ms, under the {} ms of {} tiles at walking pace.",
            NURSE_RUN_HOLD.count(), needed.count(), CENTER_MAT_TO_COUNTER_TILES);
    }
    return true;
}

std::string sandgem_heal(Logger& logger, SimCenterDoors doors, bool slow_fade_in){
    SimWorldKnobs knobs;
    knobs.slow_fade_in = slow_fade_in;
    knobs.centers["heal_sandgem"] = doors;
    return run_case(logger, "00.04b", "00.04b", {.fade = {}, .banner = {}}, std::move(knobs));
}

UnitTestResult center_run_out_stays_inside(Logger& logger){
    const std::string stopped = sandgem_heal(logger, {.stays_inside = true}, false);
    if (stopped.find(Pokemon::CENTER_NOT_LEFT) == std::string::npos){
        return std::format("the run out stayed inside, but the row {}", stopped.empty() ? std::string("passed") : stopped);
    }
    return true;
}

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

UnitTestResult moms_scene_ends_with_a_fade(Logger& logger){
    SimWorldKnobs knobs;
    knobs.scenes["rowan_and_dawn"] = {
        sim_line("Dawn"),
        sim_line("Barry"),
        sim_fade("the fade home", 1500ms),
        sim_line("Mom: the Running Shoes"),
        sim_line("Mom"),
        sim_pause("Mom stands", 4000ms, false),
        sim_fade("the scene's closing fade", 2000ms),
    };
    return passes(run_case(logger, "00.03", "00.03", {.fade = {}, .banner = {}}, knobs));
}

UnitTestResult starly_opening_after_the_starter(Logger& logger){
    SimWorldKnobs knobs;
    knobs.battle_opening_before = [](const std::string& row, const std::string& step){
        return row == "00.02" && step == "starly";
    };
    return passes(run_case(logger, "00.02", "00.02", {.fade = {}, .banner = {}}, knobs));
}

UnitTestResult trainer_battle_after_lines_on_a_step(Logger& logger){
    SimWorldKnobs knobs;
    knobs.trainer_lines_before = [](const std::string& row, const std::string& step){
        return row == "01.02" && step == "into_oreburgh";
    };
    return passes(run_case(logger, "01.02", "01.02", {.fade = {}, .banner = {}}, knobs));
}

UnitTestResult sandgem_entry_two_columns_west(Logger& logger){
    SimWorldKnobs knobs;
    knobs.lands["into_sandgem"] = GridPose{.map = "SandgemTown", .tiles = {TilePos{27, 5}}, .facing = Direction::SOUTH};
    return passes(run_case(logger, "00.04b", "00.04b", {.fade = {}, .banner = {}}, std::move(knobs)));
}

UnitTestResult home_from_each_scene_end(Logger& logger){
    std::string failures;
    for (const TilePos& tile : {TilePos{6, 8}, TilePos{5, 8}}){
        SimWorldKnobs knobs;
        knobs.lands["rowan_and_dawn"] = GridPose{.map = "PlayerHouse1F", .tiles = {tile}, .facing = Direction::EAST};
        const std::string stopped = run_case(logger, "00.03", "00.03", {.fade = {}, .banner = {}}, std::move(knobs));
        if (!stopped.empty()){
            failures += std::format("\nfrom ({}, {}): {}", tile.x, tile.y, stopped);
        }
    }
    return passes(failures);
}

std::vector<StoryCheckpoint> table_with_row(const std::string& id, const GridPose& start, const BdspStep& step){
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id == id){
            checkpoint.start = start;
            checkpoint.steps = {step};
        }
    }
    return rows;
}

UnitTestResult rowans_scene_from_each_start(Logger& logger){
    const BdspStep& walk = table_step("00.03", "into_rowans_scene");
    const GridPose trigger{.map = "Route201", .tiles = {TilePos{5, 14}, TilePos{5, 15}, TilePos{5, 16}}, .facing = Direction::EAST};
    std::string failures;
    for (const TilePos& tile : {TilePos{17, 14}, TilePos{17, 15}, TilePos{20, 21}, TilePos{20, 22}, TilePos{20, 23}}){
        const GridPose start{.map = "VerityLakefront", .tiles = {tile}, .facing = Direction::SOUTH};
        const std::vector<StoryCheckpoint> rows = table_with_row("00.03", start, walk);
        SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
        const SimRunReport report = run_rows_on(executor, logger, {.from = "00.03", .to = "00.03"}, rows);
        if (!report.completed){
            failures += std::format("\nfrom ({}, {}): stopped in {}: {}", tile.x, tile.y, executor.result().failed_step, executor.result().error);
        }else if (!ends_on(executor.result(), trigger)){
            failures += std::format("\nfrom ({}, {}): the walk ended at {}, not on Rowan's trigger", tile.x, tile.y, end_of(executor.result()));
        }
    }
    return passes(failures);
}

UnitTestResult dawns_scene_mid_run(Logger& logger){
    const BdspStep& walk = table_step("00.03", "into_dawns_scene");
    const GridPose start{.map = "TwinleafTown", .tiles = {map_point("TwinleafTown", "house_doorstep")}, .facing = Direction::SOUTH};
    const std::vector<StoryCheckpoint> rows = table_with_row("00.03", start, walk);
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
    const SimRunReport report = run_rows_on(executor, logger, {.from = "00.03", .to = "00.03"}, rows);
    if (!report.completed){
        return std::format("stopped in {}: {}", executor.result().failed_step, executor.result().error);
    }
    const GridPose trigger{.map = "SandgemTown", .tiles = {TilePos{4, 12}}, .facing = Direction::EAST};
    if (!ends_on(executor.result(), trigger)){
        return std::format("the walk ended at {}, not on Dawn's trigger (4, 12).", end_of(executor.result()));
    }
    return true;
}

SimWorldKnobs route_201_grass(){
    SimWorldKnobs knobs;
    knobs.grass.every = 8;
    return knobs;
}

std::vector<BdspStep> ledge_walk(const std::set<std::string>& skip){
    std::vector<BdspStep> steps;
    bool on = false;
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const BdspStep& step : checkpoint.steps){
            if (checkpoint.id != "00.04"){
                continue;
            }
            on |= step.name == "to_route_201";
            if (on && !skip.contains(step.name)){
                steps.push_back(step);
            }
            if (step.name == "over_the_ledge"){
                return steps;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No over_the_ledge after to_route_201 in 00.04");
}

std::string ledge_walk_in_grass(Logger& logger, const std::set<std::string>& skip){
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id == "00.04"){
            checkpoint.start = std::get<RouteStepArgs>(table_step("00.04", "onto_the_path").args).target;
            checkpoint.steps = ledge_walk(skip);
        }
    }
    return run_case(logger, "00.04", "00.04", {.fade = {}, .banner = {}}, route_201_grass(), rows);
}

std::string fought(const SimRun& run){
    std::string out;
    for (const std::string& scene : run.scenes_fought){
        out += (out.empty() ? "" : "; ") + scene;
    }
    return out.empty() ? "none" : out;
}

UnitTestResult moms_call_above_the_mat(Logger& logger){
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
    const SimRunReport report = run_rows_on(executor, logger, {.from = "00.04", .to = "00.04"});
    if (!report.completed){
        return std::format("stopped in {}: {}", executor.result().failed_step, executor.result().error);
    }
    if (!executor.result().scenes_fought.empty()){
        return "a walk fought a scene: " + fought(executor.result());
    }
    return true;
}

UnitTestResult old_walk_hands_moms_call_over(Logger& logger){
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id != "00.04"){
            continue;
        }
        std::vector<BdspStep> steps;
        for (BdspStep& step : checkpoint.steps){
            if (step.name != "into_moms_call"){
                steps.push_back(std::move(step));
                continue;
            }
            for (BdspStep& old : old_round_mom_to_the_mat()){
                steps.push_back(std::move(old));
            }
        }
        checkpoint.steps = std::move(steps);
    }
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
    const SimRunReport report = run_rows_on(executor, logger, {.from = "00.04", .to = "00.04"}, rows);
    const SimRun& run = executor.result();
    if (!report.completed){
        return std::format("stopped in {}: {}", run.failed_step, run.error);
    }
    if (!run.scenes_fought.empty()){
        return "a walk fought a scene: " + fought(run);
    }
    return true;
}

struct FailureCase{
    struct Input{
        std::function<UnitTestResult(Logger&)> body;
    };
    struct Expectation{
    };
    std::string name;
    Input input;
    Expectation expectation;
};

class Test_AutoStoryFailureCase : public UnitTest{
public:
    explicit Test_AutoStoryFailureCase(FailureCase test_case)
        : UnitTest(std::format("PokemonBDSP::AutoStory - Console failure on the sim: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return known_failing().run(logger, m_case.name, [&]{
            return m_case.input.body(logger);
        });
    }

private:
    FailureCase m_case;
};

//  01.03's walk below Oreburgh's Center ending `column` instead of on the
//  door's column 47: its tap east replaced by `legs`.
UnitTestResult oreburgh_heal_from_column(Logger& logger, int column, std::vector<Pokemon::StoryLeg> legs){
    BdspStep step = table_step("01.03", "east_below_the_centers_door");
    std::get<LegsStepArgs>(step.args).legs = std::move(legs);
    step.pose_after = GridPose{.map = "OreburghCity", .tiles = {{column, 21}}, .facing = Direction::NORTH};
    return passes(run_case(logger, "01.03", "01.03", {.fade = {}, .banner = {}}, /*knobs=*/{}, table_with("01.03", step)));
}

UnitTestResult barrys_warning_with_still_taps(Logger& logger){
    SimWorldKnobs knobs;
    knobs.still_taps = {{.step = "below_barrys_warning", .direction = Direction::NORTH, .taps = {13, 14}}};
    return passes(run_case(logger, "00.01", "00.01", {.fade = {}, .banner = {}}, std::move(knobs)));
}

} // namespace

const FailureCase AUTO_STORY_FAILURE_CASES[] = {
    {.name = "01.05 through_the_gate's last leg runs a tile on (lab-B 2026-11-08 15:31)", .input = {[](Logger& logger){
        return passes(run_case(logger, "01.05", "01.05", {.fade = {}, .banner = {}}, gate_slide(SimFade::DIM, false)));
    }}, .expectation = {}},
    {.name = "01.05 old walk slides onto the Gate's west mouth, dim fade, banner gone", .input = {[](Logger& logger){
        return gate_old_walk_slides(logger, SimFade::DIM, false);
    }}, .expectation = {}},
    {.name = "01.05 old walk slides onto the Gate's west mouth, dim fade, banner up", .input = {[](Logger& logger){
        return gate_old_walk_slides(logger, SimFade::DIM, true);
    }}, .expectation = {}},
    {.name = "01.05 old walk slides onto the Gate's west mouth, black fade", .input = {[](Logger& logger){
        return gate_old_walk_slides(logger, SimFade::BLACK, true);
    }}, .expectation = {}},
    {.name = "01.05 old walk slides onto the Gate's west mouth, white fade", .input = {[](Logger& logger){
        return gate_old_walk_slides(logger, SimFade::WHITE, true);
    }}, .expectation = {}},
    {.name = "01.05 through_the_gate is one run west", .input = {gate_plan_one_run}, .expectation = {}},
    {.name = "01.03 heal_oreburgh from a column west of the Center's door gets in (lab-B 2026-11-23 19:57, 20:04)", .input = {[](Logger& logger){
        return oreburgh_heal_from_column(logger, /*column=*/46, {{.direction = Direction::WEST, .tiles = 1}, {.direction = Direction::EAST, .tiles = 1}});
    }}, .expectation = {}},
    {.name = "01.03 heal_oreburgh from a column east of the Center's door gets in", .input = {[](Logger& logger){
        return oreburgh_heal_from_column(logger, /*column=*/48, {{.direction = Direction::EAST, .tiles = 2}});
    }}, .expectation = {}},
    {.name = "00.01 taps 13 and 14 up Twinleaf's exit read still: the run north still ends in Barry's warning (lab-B 2026-11-15)",
        .input = {barrys_warning_with_still_taps}, .expectation = {}},
    {.name = "any one missed fade or banner from 00.00 to 01.11: every row still passes", .input = {[](Logger& logger){
        return every_single_miss(logger, {});
    }}, .expectation = {}},
    {.name = "any one missed fade or banner at a warp that shows both: its row still passes", .input = {[](Logger& logger){
        return every_single_miss(logger, ONE_SIGNAL_STEPS);
    }}, .expectation = {}},
    {.name = "any one fade with its hold unseen from 00.00 to 01.11: every row still passes", .input = {every_single_hold_miss}, .expectation = {}},
    {.name = "01.02 to 01.05 at night, the Gate's dim floor", .input = {night_and_dim_floors}, .expectation = {}},
    {.name = "01.03 Roark's first line 7 s after the A press", .input = {roark_speaks_late}, .expectation = {}},
    {.name = "00.01 MASH first lines at 7 s and 9 s", .input = {mash_scene_speaks_late}, .expectation = {}},
    {.name = "01.03 with Oreburgh's people wandering (the pokefan on 45, 37..39)", .input = {[](Logger& logger){
        return wanderers_move(logger, "01.03");
    }}, .expectation = {}},
    {.name = "01.08 with Route 204's youngster wandering (12..14, 2)", .input = {[](Logger& logger){
        return wanderers_move(logger, "01.08");
    }}, .expectation = {}},
    {.name = "01.04 Roark's badge speech: the fanfare's pause, then a scene", .input = {roark_badge_speech}, .expectation = {}},
    {.name = "inside a Pokemon Center, the door's settle ends within 3 s of the fade-up with visitors walking, never during the fade", .input = {center_inside_settles}, .expectation = {}},
    {.name = "inside a Pokemon Center, one held run north from the door mat reaches the nurse", .input = {nurse_in_one_run}, .expectation = {}},
    {.name = "00.04b the Center fades in for 4 s: the walk to the nurse waits it out (lab-B 2026-11-10)", .input = {[](Logger& logger){
        return passes(sandgem_heal(logger, {.fade_in = 4000ms}, true));
    }}, .expectation = {}},
    {.name = "00.04b the Center's fades come after the runs in and out", .input = {[](Logger& logger){
        return passes(sandgem_heal(logger, {.late_fade = true}, true));
    }}, .expectation = {}},
    {.name = "00.04b the run out of the Center stops inside: heal_exit (r143a)", .input = {center_run_out_stays_inside}, .expectation = {}},
    {.name = "01.07 after the tag battle the grunts run off before anyone speaks", .input = {grunts_run_off_before_rowan}, .expectation = {}},
    {.name = "00.03 Rowan's scene opens with no box and fades home", .input = {rowans_scene_opens_with_no_box}, .expectation = {}},
    {.name = "00.03 Mom's scene ends with a closing fade: over once it is back up (lab-B 2026-11-09)", .input = {moms_scene_ends_with_a_fade}, .expectation = {}},
    {.name = "00.02 the Starly battle's opening still plays after the starter (lab-B 2026-11-09)", .input = {starly_opening_after_the_starter}, .expectation = {}},
    {.name = "01.02 a trainer's lines, then her battle, before a step with no trainer cue: fought, then the step (lab-B run 8)", .input = {trainer_battle_after_lines_on_a_step}, .expectation = {}},
    {.name = "00.03 out of the house from (6, 8), where Rowan's scene ends, and from a column west", .input = {home_from_each_scene_end}, .expectation = {}},
    {.name = "00.03 the walk into Rowan's scene routes onto his row and stops on his trigger mid-run", .input = {rowans_scene_from_each_start}, .expectation = {}},
    {.name = "00.03 the walk into Sandgem stops on Dawn's trigger mid-run", .input = {dawns_scene_mid_run}, .expectation = {}},
    {.name = "00.04 Mom's call fires above the mat, on (6, 9): the walk ends in it, her hat scene plays it (lab-B 2026-11-09 17:29)", .input = {moms_call_above_the_mat}, .expectation = {}},
    {.name = "00.04 the old walk round Mom to (6, 9) runs into her call: it hands it to her hat scene, not fights it (lab-B 2026-11-09 17:29)", .input = {old_walk_hands_moms_call_over}, .expectation = {}},
    {.name = "00.04b the run into Sandgem stops two columns west: the walk still ends below the Center's door (lab-B 2026-11-09 20:09)", .input = {sandgem_entry_two_columns_west}, .expectation = {}},
    {.name = "00.04 to the ledge column with a wild battle on each pass through Route 201's grass (lab-B 2026-11-09 16:14)", .input = {[](Logger& logger){
        return passes(ledge_walk_in_grass(logger, {}));
    }}, .expectation = {}},
    {.name = "00.04 the old walk to the ledge column, west to row 5's wall through the grass, now passes", .input = {[](Logger& logger){
        return passes(ledge_walk_in_grass(logger, {"to_the_signs_row"}));
    }}, .expectation = {}},
    {.name = "00.05b with Jubilife's people wandering (school_kid_m_5 on 51, 33..35)", .input = {[](Logger& logger){
        return wanderers_move(logger, "00.05b");
    }}, .expectation = {}},
    {.name = "00.00 to 01.02 chained with wild battles every 8 grass tiles (wild battle + trainer on the same wall leg)", .input = {[](Logger& logger){
        return passes(run_case(logger, "00.00", "01.02", {.fade = {}, .banner = {}}, route_201_grass()));
    }}, .expectation = {}},
};

namespace{

struct KnownFailingNamesCase{
    struct Input{
    };
    struct Expectation{
    };
    std::string name;
    Input input;
    Expectation expectation;
};

class Test_AutoStoryFailureCasesKnownFailing : public UnitTest{
public:
    explicit Test_AutoStoryFailureCasesKnownFailing(const KnownFailingNamesCase& test_case)
        : UnitTest(std::format("PokemonBDSP::AutoStory - Console failure on the sim: {}", test_case.name))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::set<std::string> names;
        for (const FailureCase& item : AUTO_STORY_FAILURE_CASES){
            names.insert(item.name);
        }
        const std::vector<std::string> stale = known_failing().stale(names);
        if (stale.empty()){
            return true;
        }
        std::string out = "KNOWN_FAILING names no such case:";
        for (const std::string& entry : stale){
            out += std::format("\n  {}", entry);
        }
        return out;
    }
};

const KnownFailingNamesCase KNOWN_FAILING_NAMES_CASES[] = {
    {.name = "every known failing entry names a case", .input = {}, .expectation = {}},
};

} // namespace

void add_tests_AutoStory_SimFailureCases(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_FAILURE_CASES){
        database.add<Test_AutoStoryFailureCase>(item);
    }
    for (const KnownFailingNamesCase& item : KNOWN_FAILING_NAMES_CASES){
        database.add<Test_AutoStoryFailureCasesKnownFailing>(item);
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
