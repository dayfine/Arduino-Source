/*  BDSP AutoStory Story Monte Carlo Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's rows on the story simulation (SimExecutor: the program's own
 *  walking steps on the grid sim), each run once per pinned seed with that
 *  seed's disturbances (SimDisturbance): wild battles on grass and in caves,
 *  drift off the grid, the maps' wanderers, and sometimes one missed fade or
 *  banner. Each row's test logs its pass rate and, per failing step, the
 *  failure kinds with a seed that reproduces each, and fails only below the
 *  row's pinned floor. The old 01.03 test shows the harness finds the two
 *  failures lab-B met on 2026-11-08. Each row's seeds that pass with no
 *  miss run again with one fade's hold unseen, and must pass again. With
 *  dialogs popping up between steps, a seed that passes without them
 *  must pass with them, the executor clearing them before each step.
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryMonteCarlo.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_SimDisturbance.h"
#include "PokemonBDSP_AutoStory_StorySim.h"
#include "PokemonBDSP_AutoStory_StoryMonteCarlo_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::RowReport;
using Pokemon::SeedOutcome;
using Pokemon::StoryStep;
using Pokemon::TilePos;

constexpr std::string_view SEEDS_VARIABLE = "PA_BDSP_STORY_MC_SEEDS";
constexpr std::string_view ROWS_VARIABLE = "PA_BDSP_STORY_MC_ROWS";
constexpr std::string_view TRACE_VARIABLE = "PA_BDSP_STORY_MC_TRACE";
constexpr size_t DEFAULT_SEEDS = 20;

//  The runs log nothing: a seed's own log is a reproduction away.
class SilentLogger : public Logger{
public:
    virtual void log(const std::string&, Color) override{}
};

//  The fades and banners an undisturbed run of `row` looks for.
SimMisses looked_for(const std::vector<StoryCheckpoint>& rows, const std::string& row){
    SilentLogger logger;
    SimExecutor executor(logger);
    run_rows_on(executor, logger, {.from = row, .to = row}, rows);
    return executor.misses();
}

//  How a failed run failed, as the report groups it.
SeedOutcome failed_outcome(const Pokemon::SegmentReport& report, const SimRun& run, const SimDisturbance& noise){
    const bool in_step = !run.failed_step.empty();
    SeedOutcome out{
        .passed = false,
        .failed_step = in_step ? run.failed_step : report.failed_checkpoint + " " + report.failed_event,
        .kind = in_step ? Pokemon::failure_kind(run.failure, run.failure_detail)
            : Pokemon::failure_kind(report.failure.failure, report.failure.detail),
        .detail = std::format("{}; {} wild battle(s)", describe(noise.plan()), run.battles),
    };
    if (run.failed_step_battles > 0){
        out.kind += " [wild battle in the step]";
    }
    if (in_step && run.missed_step == run.failed_step){
        out.kind += " [missed detection in the step]";
    }
    return out;
}

//  How a row's seeds are disturbed: at `rates`, missing one of the fades
//  and banners its undisturbed run looked for (`clean`).
struct RowNoise{
    SimMisses clean;
    Pokemon::DisturbanceRates rates;
};

//  The rates the harness was first built with, harsher than lab-B's (see
//  DisturbanceRates): Gen IV's encounter rates, taps that drift as the
//  stick does, and a miss in about a quarter of 01.03's seeds.
Pokemon::DisturbanceRates stress_rates(){
    Pokemon::DisturbanceRates rates;
    rates.grass_encounter = 30.0 / 180;
    rates.cave_encounter = 10.0 / 180;
    rates.tap_drift_per_tile = rates.drift_per_tile;
    rates.missed_detection = 0.04;
    return rates;
}

//  What the executor does with a dialog that pops up between steps.
enum class PoppedDialog{
    CLEARED,    //  cleared before the next step, as the console does
    LEFT_UP,    //  left up, as before the executor looked (SimWorldKnobs::clear_interrupts)
};

//  Row `row` of `rows`, disturbed by `seed`, logging to `logger`.
SeedOutcome run_seed(
    Logger& logger, const std::vector<StoryCheckpoint>& rows, const RowNoise& noise_of, const std::string& row, uint64_t seed,
    PoppedDialog popped = PoppedDialog::CLEARED
){
    const SimMisses& clean = noise_of.clean;
    SimDisturbance noise(Pokemon::make_disturbance_plan(seed, noise_of.rates, clean.fades, clean.banners));
    Pokemon::SimRandom dialogs = noise.plan().dialogs;
    const double dialog_rate = noise_of.rates.dialog_between_steps;
    SimWorldKnobs knobs{
        .dialog_before = [&dialogs, dialog_rate](const std::string&, const std::string&){
            return dialog_rate > 0 && dialogs.chance(dialog_rate);
        },
        .clear_interrupts = popped == PoppedDialog::CLEARED,
        .noise = &noise,
    };
    SimExecutor executor(logger, {.fade = noise.plan().missed_fade, .banner = noise.plan().missed_banner, .hold = noise.plan().missed_hold}, std::move(knobs));
    try{
        const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = row, .to = row}, rows);
        logger.log(std::format("[Sim] {}: {} wild battle(s) over {} grass and {} cave tiles.", describe(noise.plan()),
            noise.battles(), noise.encounter_tiles().grass, noise.encounter_tiles().cave));
        return report.completed ? SeedOutcome{.passed = true} : failed_outcome(report, executor.result(), noise);
    }catch (const GridSimFailure& e){
        return {.failed_step = row + " (thrown)", .kind = Pokemon::failure_kind("sim_failure", e.message), .detail = describe(noise.plan())};
    }catch (const Exception& e){
        return {.failed_step = row + " (thrown)", .kind = Pokemon::failure_kind(e.name(), e.message()), .detail = describe(noise.plan())};
    }
}

RowReport run_row(const std::vector<StoryCheckpoint>& rows, const std::string& row, size_t seeds,
    const Pokemon::DisturbanceRates& rates = {}
){
    const RowNoise noise{.clean = looked_for(rows, row), .rates = rates};
    return Pokemon::run_row_seeds(row, Pokemon::story_seeds(seeds), [&](const std::string& id, uint64_t seed){
        SilentLogger logger;
        return run_seed(logger, rows, noise, id, seed);
    });
}


//  A row's seeds (run_row()), and each that passes with no miss run again
//  with one fade's hold dropped (a short fade between two frames; the
//  door's watcher missed 13 of 35 on lab-B, 2026-11-08): the program's fade
//  watch takes the scene after the fall, so it must pass again. How many
//  ran again, and those that then failed, as "plan: step kind".
struct HeldRowReport{
    RowReport report;
    size_t rerun = 0;
    std::vector<std::string> hold_failed;
};
HeldRowReport run_row_holding(const std::vector<StoryCheckpoint>& rows, const std::string& row, size_t seeds){
    const RowNoise noise{.clean = looked_for(rows, row), .rates = {}};
    RowNoise hold_noise = noise;
    hold_noise.rates.missed_hold = 1;
    HeldRowReport out;
    out.report = Pokemon::run_row_seeds(row, Pokemon::story_seeds(seeds), [&](const std::string& id, uint64_t seed){
        SilentLogger silent;
        const SeedOutcome plain = run_seed(silent, rows, noise, id, seed);
        const Pokemon::DisturbancePlan plan = Pokemon::make_disturbance_plan(seed, noise.rates, noise.clean.fades, noise.clean.banners);
        if (plain.passed && noise.clean.fades > 0 && !plan.missed_fade && !plan.missed_banner){
            out.rerun++;
            const SeedOutcome hold = run_seed(silent, rows, hold_noise, id, seed);
            if (!hold.passed){
                out.hold_failed.push_back(std::format("{}: {} {}", hold.detail, hold.failed_step, hold.kind));
            }
        }
        return plain;
    });
    return out;
}


//  Row `row` for the seeds, under `floor` passes in a hundred a failure:
//  each row's floor is what it passed at this branch's tip, so a row that
//  passes less often has got worse. A row PA_BDSP_STORY_MC_ROWS leaves out
//  is skipped.
UnitTestResult test_row(Logger& logger, const std::string& row, size_t floor){
    if (Pokemon::selected_rows(ROWS_VARIABLE, {row}).empty()){
        return {UnitTestResult::SKIPPED, std::string(ROWS_VARIABLE) + " leaves it out"};
    }
    //  One seed's run, logged step by step: how a failure the report
    //  names came about.
    if (const size_t traced = Pokemon::seed_count(TRACE_VARIABLE, 0); traced > 0){
        const std::vector<StoryCheckpoint> rows = story_checkpoints();
        const SeedOutcome out = run_seed(logger, rows, {.clean = looked_for(rows, row), .rates = {}}, row, traced);
        logger.log(std::format("{} seed {}: {} {} {}", row, traced, out.passed ? "passed" : "failed", out.failed_step, out.kind));
        return true;
    }
    const size_t seeds = Pokemon::seed_count(SEEDS_VARIABLE, DEFAULT_SEEDS);
    const WallClock start = current_time();
    const HeldRowReport held = run_row_holding(story_checkpoints(), row, seeds);
    const RowReport& report = held.report;
    const std::vector<std::string>& hold_failed = held.hold_failed;
    const auto took = std::chrono::duration_cast<Milliseconds>(current_time() - start);
    logger.log(Pokemon::format_report(report) + std::format("\n    ({} seeds in {} ms)", seeds, took.count()));
    logger.log(std::format("{}: one fade's hold unseen: {} of {} seeds that passed with no miss fail.", row, hold_failed.size(), held.rerun));
    if (!hold_failed.empty()){
        std::string out = std::format("{} fails with one fade's hold unseen where it passed without:", row);
        for (const std::string& line : hold_failed){
            out += "\n  " + line;
        }
        return out;
    }
    //  The floors are the default seeds' counts; other counts only report.
    if (seeds == DEFAULT_SEEDS && report.passes * 100 < floor * report.runs){
        return std::format("{} passed {} of {}, under its floor of {}%:\n{}", row, report.passes, report.runs,
            floor, Pokemon::format_report(report));
    }
    return true;
}


//  The chance a dialog pops up before a step, in the test that pops them,
//  and its seeds a row: each runs three times, so fewer than a row's own.
constexpr double DIALOG_RATE = 0.1;
constexpr size_t DIALOG_SEEDS = 5;

//  How a row's seeds fare with dialogs popping up between steps: without
//  them, with them cleared before each step, and with them left up (the
//  executor before it looked). Seeds that pass without them and fail with
//  them cleared, as "row plan: step kind".
struct DialogRowReport{
    size_t runs = 0;
    size_t plain = 0;
    size_t cleared = 0;
    size_t left = 0;
    std::vector<std::string> regressions;
};
void run_row_dialogs(DialogRowReport& out, const std::vector<StoryCheckpoint>& rows, const std::string& row, size_t seeds){
    const RowNoise plain{.clean = looked_for(rows, row), .rates = {}};
    RowNoise popping = plain;
    popping.rates.dialog_between_steps = DIALOG_RATE;
    for (uint64_t seed : Pokemon::story_seeds(seeds)){
        SilentLogger silent;
        const bool passes = run_seed(silent, rows, plain, row, seed).passed;
        const SeedOutcome cleared = run_seed(silent, rows, popping, row, seed);
        const bool left = run_seed(silent, rows, popping, row, seed, PoppedDialog::LEFT_UP).passed;
        out.runs++;
        out.plain += passes ? 1 : 0;
        out.cleared += cleared.passed ? 1 : 0;
        out.left += left ? 1 : 0;
        if (passes && !cleared.passed){
            out.regressions.push_back(std::format("{} {}: {} {}", row, cleared.detail, cleared.failed_step, cleared.kind));
        }
    }
}

//  Every row to 01.11, its seeds with dialogs popping up between steps
//  (DIALOG_RATE a step): one that passes without them must pass with them.
UnitTestResult test_dialogs_between_steps(Logger& logger){
    const std::vector<StoryCheckpoint> rows = story_checkpoints();
    const size_t seeds = Pokemon::seed_count(SEEDS_VARIABLE, DIALOG_SEEDS);
    DialogRowReport report;
    for (const StoryCheckpoint& row : rows){
        if (row.id > "01.11"){
            continue;
        }
        run_row_dialogs(report, rows, row.id, seeds);
    }
    logger.log(std::format(
        "Dialogs between steps, {} runs: {} pass without them, {} with them cleared, {} with them left up.",
        report.runs, report.plain, report.cleared, report.left
    ));
    if (report.regressions.empty()){
        return true;
    }
    std::string out = "Seeds that pass without dialogs between steps fail with them cleared:";
    for (const std::string& line : report.regressions){
        out += "\n  " + line;
    }
    return out;
}


//  01.03's steps across Roark's room as they were before 4f13c19bb:
//  counted taps onto Roark's row, then west against his rock.
std::vector<StoryStep> old_steps_to_roark(){
    StoryStep taps;
    taps.kind = Pokemon::StoryStepKind::LEGS;
    taps.name = "above_roarks_row";
    taps.action = "legs";
    taps.args["legs"] = std::vector<Pokemon::StoryLeg>{
        {.direction = Direction::EAST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 2},
        {.direction = Direction::WEST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 5},
        {.direction = Direction::EAST, .tiles = 14}, {.direction = Direction::SOUTH, .tiles = 10},
        {.direction = Direction::WEST, .tiles = 2}, {.direction = Direction::SOUTH, .tiles = 2},
    };
    taps.pose_after = Pokemon::TileDeclared{.map = "OreburghMine", .tiles = {TilePos{27, 28}}, .facing = Direction::SOUTH};
    StoryStep walls;
    walls.kind = Pokemon::StoryStepKind::LEGS;
    walls.name = "beside_roarks_rock";
    walls.action = "to_wall";
    walls.args["legs"] = std::vector<Pokemon::StoryLeg>{{.direction = Direction::WEST, .tiles = 7}, {.direction = Direction::SOUTH, .tiles = 1}};
    walls.pose_after = Pokemon::TileDeclared{.map = "OreburghMine", .tiles = {TilePos{20, 29}}, .facing = Direction::SOUTH};
    return {taps, walls};
}

//  01.03 as it was before be57db157 and 4f13c19bb: counted taps across
//  Roark's room, and the route to below the Center's door straight into
//  the heal.
std::vector<StoryCheckpoint> old_0103(){
    const std::vector<StoryStep> old = old_steps_to_roark();
    const std::vector<std::string> added{"south_of_the_centers_door", "against_the_walls_east_of_the_door", "below_the_centers_door_lined_up"};
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& row : rows){
        if (row.id != "01.03"){
            continue;
        }
        std::erase_if(row.steps, [&](const StoryStep& step){
            return std::find(added.begin(), added.end(), step.name) != added.end();
        });
        std::replace_if(row.steps.begin(), row.steps.end(), [](const StoryStep& step){ return step.name == "to_roarks_corner"; }, old[0]);
        std::replace_if(row.steps.begin(), row.steps.end(), [](const StoryStep& step){ return step.name == "beside_roarks_rock"; }, old[1]);
    }
    return rows;
}

//  Row 01.03 of `rows` from out of the mine on: the walk up to below the
//  Center's door and the heal, without the mine's failures before it.
std::vector<StoryCheckpoint> from_the_mines_sign(std::vector<StoryCheckpoint> rows){
    for (StoryCheckpoint& row : rows){
        if (row.id != "01.03"){
            continue;
        }
        const auto sign = std::find_if(row.steps.begin(), row.steps.end(), [](const StoryStep& step){
            return step.name == "up_to_the_mines_sign";
        });
        row.start = Pokemon::TileDeclared{.map = "OreburghCity", .tiles = {TilePos{45, 59}, TilePos{45, 58}}, .facing = Direction::NORTH};
        row.saves_in.clear();
        row.steps.erase(row.steps.begin(), sign);
    }
    return rows;
}

//  Whether `report` failed `step` in a way whose kind holds `text` and `also`.
bool failed_with(const RowReport& report, const std::string& step, const std::string& text, const std::string& also = ""){
    const auto found = report.failures.find(step);
    return found != report.failures.end() && std::ranges::any_of(found->second, [&](const auto& kind){
        return kind.first.find(text) != std::string::npos && kind.first.find(also) != std::string::npos;
    });
}

//  lab-B 2026-11-08: a wild battle on the counted taps across Roark's room
//  re-planned to an open leg and the row went one off (11:55); a run up
//  Oreburgh's column 46 stopped short and an open step east left us off the
//  Center door's column (10:39). The old row must show both, at the stress
//  rates: at lab-B's own, 200 seeds need not draw them.
UnitTestResult test_old_0103(Logger& logger){
    const size_t seeds = std::max<size_t>(200, Pokemon::seed_count(SEEDS_VARIABLE, DEFAULT_SEEDS));
    const RowReport whole = run_row(old_0103(), "01.03", seeds, stress_rates());
    logger.log("The old 01.03: " + Pokemon::format_report(whole));
    const RowReport town = run_row(from_the_mines_sign(old_0103()), "01.03", seeds, stress_rates());
    logger.log("The old 01.03 from the mine's sign: " + Pokemon::format_report(town));
    logger.log("Today's 01.03 from the mine's sign: " + Pokemon::format_report(
        run_row(from_the_mines_sign(story_checkpoints()), "01.03", seeds, stress_rates())));
    //  A battle on the taps, then the row ends off the tile the taps count.
    const bool roark = failed_with(whole, "01.03 above_roarks_row", "sim_mismatch", "[wild battle in the step]");
    //  From the mine's warp (the whole row) or the sign.
    const bool center = failed_with(whole, "01.03 heal_oreburgh", "no door north") || failed_with(town, "01.03 heal_oreburgh", "no door north");
    if (!roark || !center){
        return std::format("expected the old 01.03 to fail across Roark's room after a wild battle ({}) and at the Center's door ({}):\n{}\n{}",
            roark, center, Pokemon::format_report(whole), Pokemon::format_report(town));
    }
    return true;
}


//  A disturbed hold with no disturbance walks as the grid sim's own hold
//  does, at its pace: a hold timed a tile at the walker's 160 ms east
//  moved no tile at the sim's 200 ms (fe2da03e7), and every hold east or
//  west at a walk stood still (00.05 east_to_the_wall, 2026-11-12).
UnitTestResult test_disturbed_hold_as_plain(Logger& logger){
    const StoryWorld* world = story_plan_world("JubilifeCity", "JubilifeCity");
    if (world == nullptr){
        return "JubilifeCity world not found";
    }
    struct Hold{
        Direction direction;
        SimPace pace;
        bool can_run;
    };
    const Hold holds[] = {
        {Direction::EAST, SimPace::WALK, false}, {Direction::WEST, SimPace::WALK, false},
        {Direction::NORTH, SimPace::WALK, false}, {Direction::EAST, SimPace::RUN, true},
    };
    Pokemon::DisturbanceRates still;
    still.grass_encounter = 0;
    still.cave_encounter = 0;
    still.drift_per_tile = 0;
    const TilePos start = story_to_world(*world, "JubilifeCity", {46, 54});
    for (const Hold& hold : holds){
        GridSim plain({.world = world}, start, hold.direction, hold.can_run);
        const SimMove expected = plain.hold(hold.direction, 10000ms, hold.pace);
        GridSim disturbed({.world = world}, start, hold.direction, hold.can_run);
        SimDisturbance noise(Pokemon::make_disturbance_plan(1, still, 0, 0));
        const SimMove got = disturbed_hold(disturbed, &noise, hold.direction, 10000ms, hold.pace).move;
        logger.log(std::format("{}: plain {} to {}, disturbed {} to {}.", direction_name(hold.direction),
            expected.distance / SIM_TILE, to_string(plain.player().pos), got.distance / SIM_TILE, to_string(disturbed.player().pos)));
        if (got.distance != expected.distance || got.stop != expected.stop || disturbed.player().pos != plain.player().pos){
            return std::format("{} held: the disturbed hold moved {} where the plain one moved {}.",
                direction_name(hold.direction), got.distance / SIM_TILE, expected.distance / SIM_TILE);
        }
    }
    return true;
}


//  A row of the table, disturbed; or, with a body, another test.
struct StoryMonteCarloInput{
    std::string row;
    std::function<UnitTestResult(Logger&)> body = nullptr;
};

//  The fewest passes in a hundred a row's seeds may have.
struct StoryMonteCarloExpectation{
    size_t floor = 0;
};

struct StoryMonteCarloCase{
    StoryMonteCarloInput input;
    StoryMonteCarloExpectation expectation;
};

class Test_StoryMonteCarlo : public UnitTest{
public:
    explicit Test_StoryMonteCarlo(StoryMonteCarloCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Story Monte Carlo: " + test_case.input.row)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body ? m_case.input.body(logger)
                : test_row(logger, m_case.input.row, m_case.expectation.floor);
        }catch (const GridSimFailure& e){
            return "GridSimFailure: " + e.message;
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    StoryMonteCarloCase m_case;
};

} // namespace


const StoryMonteCarloCase AUTO_STORY_MONTE_CARLO_CASES[] = {
    {.input = {"00.00"}, .expectation = {.floor = 100}},
    {.input = {"00.01"}, .expectation = {.floor = 95}},
    {.input = {"00.02"}, .expectation = {.floor = 100}},
    {.input = {"00.03"}, .expectation = {.floor = 95}},
    {.input = {"00.04"}, .expectation = {.floor = 95}},
    {.input = {"00.04b"}, .expectation = {.floor = 100}},
    {.input = {"00.04c"}, .expectation = {.floor = 100}},
    {.input = {"00.04d"}, .expectation = {.floor = 100}},
    {.input = {"00.05"}, .expectation = {.floor = 100}},
    {.input = {"00.05b"}, .expectation = {.floor = 85}},
    {.input = {"00.05c"}, .expectation = {.floor = 100}},
    {.input = {"01.01"}, .expectation = {.floor = 100}},
    {.input = {"01.02"}, .expectation = {.floor = 85}},
    {.input = {"01.02b"}, .expectation = {.floor = 90}},
    {.input = {"01.03"}, .expectation = {.floor = 75}},
    {.input = {"01.04"}, .expectation = {.floor = 100}},
    {.input = {"01.05"}, .expectation = {.floor = 100}},
    {.input = {"01.06"}, .expectation = {.floor = 95}},
    {.input = {"01.07"}, .expectation = {.floor = 100}},
    {.input = {"01.08"}, .expectation = {.floor = 40}},
    {.input = {"01.09"}, .expectation = {.floor = 100}},
    {.input = {"01.10"}, .expectation = {.floor = 80}},
    {.input = {"01.11"}, .expectation = {.floor = 100}},
    {
        .input = {"a disturbed hold with no disturbance walks as the plain hold does", test_disturbed_hold_as_plain},
        .expectation = {},
    },
    {
        .input = {"the old 01.03 fails across Roark's room and at the Center's door (lab-B, 2026-11-08)", test_old_0103},
        .expectation = {},
    },
    {
        .input = {"dialogs pop up between steps, 00.00 to 01.11", test_dialogs_between_steps},
        .expectation = {},
    },
};

void add_tests_AutoStory_StoryMonteCarlo(UnitTestDatabase& database){
    for (const StoryMonteCarloCase& item : AUTO_STORY_MONTE_CARLO_CASES){
        database.add<Test_StoryMonteCarlo>(item);
    }
}




} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
