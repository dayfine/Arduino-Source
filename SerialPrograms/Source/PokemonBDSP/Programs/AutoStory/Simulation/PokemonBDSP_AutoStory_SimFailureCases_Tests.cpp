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

#include <cstdio>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Center.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_OldStorySteps.h"
#include "PokemonBDSP_AutoStory_StorySim.h"
#include "PokemonBDSP_AutoStory_SimFailureCases_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace detail;

namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::Pose;
using Pokemon::story_arg;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;

//  Cases the program fails today, with what is to fix each.
const std::map<std::string, std::string> KNOWN_FAILING{
    {"01.05 through_the_gate is one run west", "planner: goal kinds and ms costs (PLANNER_DESIGN.md)"},
    {"any one missed fade or banner from 00.00 to 01.11: every row still passes",
        "a third signal where a warp gives one (ONE_SIGNAL_STEPS): position (PLANNER_DESIGN.md 2, 4)"},
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
    step.args["target"] = GridPose{"OreburghGate", {{4, 22}}, Direction::WEST};
    return step;
}

//  01.05's walk west through the Gate whose last open leg runs a tile on;
//  the old walk's slides onto the west mouth's warp to Route 203.
SimWorldKnobs gate_slide(SimFade fade, bool banner_up){
    const Pose far = Pokemon::pose_of(*table_step("01.05", "out_of_oreburgh_gate").pose_after);
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
    const auto plan = plan_recovered_route(*world, from, [&](const TilePos& pos){ return pos == goal; }, *story_route_costs(table_step("01.05", "through_the_gate").action));
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

//  The door's settle inside a Pokemon Center (TalkOverRule::door; the
//  heal's, lab-B 2026-11-10 00.04b and 2026-11-11 01:28, where the talk's
//  5 s quiet took 18 s with visitors about). Visitors walking about, even
//  right past the player, must not hold it up: over within 3 s of the fade
//  back up. And it is never over while the fade still shows, however long
//  the fade-in runs: a walk then goes nowhere.
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
        const std::string over = play_scene(scene, {.door = true});
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

//  From the door mat, one held run north reaches the nurse's counter, even
//  at walking pace: the counter is the wall that stops it, and A talks.
UnitTestResult nurse_in_one_run(Logger&){
    const Milliseconds needed = ROUTE_START_DELAY + route_ms_per_tile(Direction::NORTH) * (int64_t)CENTER_MAT_TO_COUNTER_TILES;
    if (NURSE_RUN_HOLD < needed){
        return std::format("the run to the nurse holds {} ms, under the {} ms of {} tiles at walking pace.",
            NURSE_RUN_HOLD.count(), needed.count(), CENTER_MAT_TO_COUNTER_TILES);
    }
    return true;
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

//  00.03: Mom's lines, a still moment, then the scene's closing fade
//  (lab-B 2026-11-09 14:33). The scene once ended as the fade went down,
//  and the next step's input landed in it: the talk-over must wait for the
//  fade to come back up and for the quiet after it.
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

//  00.04b: the run down Route 202's road into Sandgem stopped two columns
//  west of the table's (29, 5), on (27, 5), and the counted walk on to
//  the Center's door ended below its west wall: the heal ran north into
//  the wall (lab-B 2026-11-09 20:09). The walk to the door must find its
//  column on walls, wherever on the road's last row the run stopped.
UnitTestResult sandgem_entry_two_columns_west(Logger& logger){
    SimWorldKnobs knobs;
    knobs.lands["into_sandgem"] = Pokemon::tile_pose("SandgemTown", {TilePos{27, 5}}, Direction::SOUTH);
    return passes(run_case(logger, "00.04b", "00.04b", {.fade = {}, .banner = {}}, std::move(knobs)));
}

//  00.05c on lab-B 2026-11-11: 00.05b's Poketch Company clown was
//  "answered" by the sign's text, so the save held two coupons; the
//  president gave no Poketch and 00.05c went on to the street as passed.
//  `misread` re-runs that read in 00.05c's own talk too. "" when the run
//  stopped at 00.05c's Poketch check, as it must when a coupon is still
//  missing.
std::string poketch_run(Logger& logger, bool misread){
    SimWorldKnobs knobs;
    knobs.coupons_at_start = {"mart_clown", "tv_clown"};
    knobs.poketch_at_start = false;
    if (misread){
        knobs.talks_without_coupon = {"company_clown"};
    }
    return run_case(logger, "00.05c", "00.05c", {.fade = {}, .banner = {}}, std::move(knobs));
}
UnitTestResult no_poketch_fails_00_05c(Logger& logger){
    const std::string stopped = poketch_run(logger, true);
    if (stopped.find("00.05c poketch") == std::string::npos){
        return UnitTestResult("expected a stop at 00.05c's Poketch check; got " + (stopped.empty() ? std::string("a pass") : stopped));
    }
    return true;
}
UnitTestResult missing_coupon_collected_in_00_05c(Logger& logger){
    return passes(poketch_run(logger, false));
}
//  01.01 from a save with no Poketch: Jubilife's east-exit guard would
//  ask about the Poketch campaign and turn us back (lab-B 2026-11-11, the
//  walk took him for a trainer); the row stops at its first step instead.
UnitTestResult gate_guard_stops_01_01(Logger& logger){
    SimWorldKnobs knobs;
    knobs.poketch_at_start = false;
    const std::string stopped = run_case(logger, "01.01", "01.01", {.fade = {}, .banner = {}}, std::move(knobs));
    if (stopped.find("01.01 poketch_before_the_gate") == std::string::npos){
        return UnitTestResult("expected a stop at 01.01's Poketch check; got " + (stopped.empty() ? std::string("a pass") : stopped));
    }
    return true;
}

//  00.03: Rowan's scene ends at home on (6, 8) beside Mom (lab-B
//  2026-11-09, 7 of 7 runs). The way out holds south onto the door's tile
//  (6, 10); from a column west, (5, 8), the hold stops on (5, 10) with no
//  fade, and a tap east onto the mat goes out.
UnitTestResult home_from_each_scene_end(Logger& logger){
    std::string failures;
    for (const TilePos& tile : {TilePos{6, 8}, TilePos{5, 8}}){
        SimWorldKnobs knobs;
        knobs.lands["rowan_and_dawn"] = Pokemon::tile_pose("PlayerHouse1F", {tile}, Direction::EAST);
        const std::string stopped = run_case(logger, "00.03", "00.03", {.fade = {}, .banner = {}}, std::move(knobs));
        if (!stopped.empty()){
            failures += std::format("\nfrom ({}, {}): {}", tile.x, tile.y, stopped);
        }
    }
    return passes(failures);
}

//  The table with row `id` cut to `step` alone, from `start`.
std::vector<StoryCheckpoint> table_with_row(const std::string& id, const Pose& start, const StoryStep& step){
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id == id){
            checkpoint.start = Pokemon::TileDeclared{start.map, Pokemon::tiles_of(start), Pokemon::facing_of(start)};
            checkpoint.steps = {step};
        }
    }
    return rows;
}
//  00.03: Rowan's trigger is the column (5, 14..16) on Route 201 (decomp),
//  20 tiles east of the Lakefront's wall row 21 (Route 201's row 14). The
//  step routes from where the lake's scene leaves us, or from a row or two
//  lower (21..23), onto that row, then runs east until his scene stops the
//  run on a tile of the column.
UnitTestResult rowans_scene_from_each_start(Logger& logger){
    const StoryStep& walk = table_step("00.03", "into_rowans_scene");
    const Pose trigger = Pokemon::tile_pose("Route201", {TilePos{5, 14}, TilePos{5, 15}, TilePos{5, 16}}, Direction::EAST);
    std::string failures;
    for (const TilePos& tile : {TilePos{17, 14}, TilePos{17, 15}, TilePos{20, 21}, TilePos{20, 22}, TilePos{20, 23}}){
        const Pose start = Pokemon::tile_pose("VerityLakefront", {tile}, Direction::SOUTH);
        const std::vector<StoryCheckpoint> rows = table_with_row("00.03", start, walk);
        SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
        const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = "00.03", .to = "00.03"}, rows);
        if (!report.completed){
            failures += std::format("\nfrom ({}, {}): stopped in {}: {}", tile.x, tile.y, executor.result().failed_step, executor.result().error);
        }else if (!ends_on(executor.result(), trigger)){
            failures += std::format("\nfrom ({}, {}): the walk ended at {}, not on Rowan's trigger", tile.x, tile.y, end_of(executor.result()));
        }
    }
    return passes(failures);
}
//  00.03: Dawn's scene starts on Sandgem's column 4 (rows 10..15, decomp),
//  five tiles into the run east from Route 201's edge (Sandgem (-1, 12) is
//  Route 201 (63, 5)). From the house's doorstep, the step routes to the
//  edge and runs on until her scene stops it there (r35, r36: an exact
//  tile and banner raced her scene).
UnitTestResult dawns_scene_mid_run(Logger& logger){
    const StoryStep& walk = table_step("00.03", "into_dawns_scene");
    const Pose start = Pokemon::tile_pose("TwinleafTown", {map_point("TwinleafTown", "house_doorstep")}, Direction::SOUTH);
    const std::vector<StoryCheckpoint> rows = table_with_row("00.03", start, walk);
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = "00.03", .to = "00.03"}, rows);
    if (!report.completed){
        return std::format("stopped in {}: {}", executor.result().failed_step, executor.result().error);
    }
    const Pose trigger = Pokemon::tile_pose("SandgemTown", {TilePos{4, 12}}, Direction::EAST);
    if (!ends_on(executor.result(), trigger)){
        return std::format("the walk ended at {}, not on Dawn's trigger (4, 12).", end_of(executor.result()));
    }
    return true;
}

//  Route 201's grass, as the console met it: a wild battle on each of four
//  passes of 10 tiles of row 5's grass (lab-B 2026-11-09 16:14). A battle
//  each 8 grass tiles gives one on every such pass.
SimWorldKnobs route_201_grass(){
    SimWorldKnobs knobs;
    knobs.grass.every = 8;
    return knobs;
}
//  00.04's steps from the east end of Sandgem's path down over Route 201's
//  ledge: "to_route_201" through "over_the_ledge", less `skip`. The run
//  west leaves us on one of seven tiles, as on the console.
std::vector<StoryStep> ledge_walk(const std::set<std::string>& skip){
    std::vector<StoryStep> steps;
    bool on = false;
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
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
//  00.04 from the east end of Sandgem's path to below Route 201's ledge, a
//  wild battle each 8 grass tiles: "" if it gets there, else where and why
//  it stopped. Without `skip`, the table's walk.
std::string ledge_walk_in_grass(Logger& logger, const std::set<std::string>& skip){
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id == "00.04"){
            const GridPose target = story_arg<GridPose>(table_step("00.04", "onto_the_path").args, "target");
            checkpoint.start = Pokemon::TileDeclared{target.map, target.tiles, target.facing};
            checkpoint.steps = ledge_walk(skip);
        }
    }
    return run_case(logger, "00.04", "00.04", {.fade = {}, .banner = {}}, route_201_grass(), rows);
}

//  The scenes a run's walks fought as trainers', or "none".
std::string fought(const SimRun& run){
    std::string out;
    for (const std::string& scene : run.scenes_fought){
        out += (out.empty() ? "" : "; ") + scene;
    }
    return out.empty() ? "none" : out;
}
//  00.04: Mom's call after her heal fires on (6, 9), above the mat
//  (lab-B 2026-11-09 17:29). The walk into it ends in her call, which her
//  hat scene's step plays to the fade out with her; no walk fights it.
UnitTestResult moms_call_above_the_mat(Logger& logger){
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = "00.04", .to = "00.04"});
    if (!report.completed){
        return std::format("stopped in {}: {}", executor.result().failed_step, executor.result().error);
    }
    if (!executor.result().scenes_fought.empty()){
        return "a walk fought a scene: " + fought(executor.result());
    }
    return true;
}
//  00.04's old walk round Mom to (6, 9) and step onto the mat. On the
//  console the walk fought her call, the step went out of the door and her
//  hat scene saw no fade. A route before a step onto a scene's trigger now
//  hands a scene that starts on the way to the scene's step, and the step
//  onto the trigger is not taken.
UnitTestResult old_walk_hands_moms_call_over(Logger& logger){
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id != "00.04"){
            continue;
        }
        std::vector<StoryStep> steps;
        for (StoryStep& step : checkpoint.steps){
            if (step.name != "into_moms_call"){
                steps.push_back(std::move(step));
                continue;
            }
            for (StoryStep& old : old_round_mom_to_the_mat()){
                steps.push_back(std::move(old));
            }
        }
        checkpoint.steps = std::move(steps);
    }
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = "00.04", .to = "00.04"}, rows);
    const SimRun& run = executor.result();
    if (!report.completed){
        return std::format("stopped in {}: {}", run.failed_step, run.error);
    }
    if (!run.scenes_fought.empty()){
        return "a walk fought a scene: " + fought(run);
    }
    return true;
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
    {.input = {"inside a Pokemon Center, the door's settle ends within 3 s of the fade-up with visitors walking, never during the fade", center_inside_settles}, .expectation = {}},
    {.input = {"inside a Pokemon Center, one held run north from the door mat reaches the nurse", nurse_in_one_run}, .expectation = {}},
    {.input = {"01.07 after the tag battle the grunts run off before anyone speaks", grunts_run_off_before_rowan}, .expectation = {}},
    {.input = {"00.03 Rowan's scene opens with no box and fades home", rowans_scene_opens_with_no_box}, .expectation = {}},
    {.input = {"00.03 Mom's scene ends with a closing fade: over once it is back up (lab-B 2026-11-09)", moms_scene_ends_with_a_fade}, .expectation = {}},
    {.input = {"00.02 the Starly battle's opening still plays after the starter (lab-B 2026-11-09)", starly_opening_after_the_starter}, .expectation = {}},
    {.input = {"00.03 out of the house from (6, 8), where Rowan's scene ends, and from a column west", home_from_each_scene_end}, .expectation = {}},
    {.input = {"00.03 the walk into Rowan's scene routes onto his row and stops on his trigger mid-run", rowans_scene_from_each_start}, .expectation = {}},
    {.input = {"00.03 the walk into Sandgem stops on Dawn's trigger mid-run", dawns_scene_mid_run}, .expectation = {}},
    {.input = {"00.04 Mom's call fires above the mat, on (6, 9): the walk ends in it, her hat scene plays it (lab-B 2026-11-09 17:29)", moms_call_above_the_mat}, .expectation = {}},
    {.input = {"00.04 the old walk round Mom to (6, 9) runs into her call: it hands it to her hat scene, not fights it (lab-B 2026-11-09 17:29)", old_walk_hands_moms_call_over}, .expectation = {}},
    {.input = {"00.04b the run into Sandgem stops two columns west: the walk still ends below the Center's door (lab-B 2026-11-09 20:09)", sandgem_entry_two_columns_west}, .expectation = {}},
    {.input = {"00.05c a coupon missing (the sign read for the Poketch Company clown): no Poketch, 00.05c fails (lab-B 2026-11-11)", no_poketch_fails_00_05c}, .expectation = {}},
    {.input = {"00.05c a coupon missing from 00.05b: its clown talks collect it, the president gives the Poketch", missing_coupon_collected_in_00_05c}, .expectation = {}},
    {.input = {"01.01 with no Poketch: the east-exit guard's gate stops the row at its check, not a trainer fight (lab-B 2026-11-11)", gate_guard_stops_01_01}, .expectation = {}},
    {.input = {"00.04 to the ledge column with a wild battle on each pass through Route 201's grass (lab-B 2026-11-09 16:14)", [](Logger& logger){
        return passes(ledge_walk_in_grass(logger, {}));
    }}, .expectation = {}},
    //  It failed there with "too many plans" until replans crossed the
    //  grass on legs to a wall (replan_costs(), 2026-11-09).
    {.input = {"00.04 the old walk to the ledge column, west to row 5's wall through the grass, now passes", [](Logger& logger){
        return passes(ledge_walk_in_grass(logger, {"to_the_signs_row"}));
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
