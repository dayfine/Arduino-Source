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
 *  failures lab-B met on 2026-11-08.
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryMonteCarlo.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP_AutoStory_SimDisturbance.h"
#include "PokemonBDSP_AutoStory_StorySim.h"
#include "PokemonBDSP_AutoStory_StoryMonteCarlo_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

using Pokemon::Direction;
using Pokemon::RowReport;
using Pokemon::SeedOutcome;
using Pokemon::StoryStep;
using Pokemon::TilePos;

constexpr const char* SEEDS_VARIABLE = "PA_BDSP_STORY_MC_SEEDS";
constexpr const char* ROWS_VARIABLE = "PA_BDSP_STORY_MC_ROWS";
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

//  Row `row` of `rows`, disturbed by `seed`.
SeedOutcome run_seed(const std::vector<StoryCheckpoint>& rows, const SimMisses& clean, const std::string& row, uint64_t seed){
    SimDisturbance noise(Pokemon::make_disturbance_plan(seed, Pokemon::DisturbanceRates{}, clean.fades, clean.banners));
    SilentLogger logger;
    SimExecutor executor(logger, {.fade = noise.plan().missed_fade, .banner = noise.plan().missed_banner}, &noise);
    try{
        const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = row, .to = row}, rows);
        return report.completed ? SeedOutcome{.passed = true} : failed_outcome(report, executor.result(), noise);
    }catch (const GridSimFailure& e){
        return {.failed_step = row + " (thrown)", .kind = Pokemon::failure_kind("sim_failure", e.message), .detail = describe(noise.plan())};
    }catch (const Exception& e){
        return {.failed_step = row + " (thrown)", .kind = Pokemon::failure_kind(e.name(), e.message()), .detail = describe(noise.plan())};
    }
}

RowReport run_row(const std::vector<StoryCheckpoint>& rows, const std::string& row, size_t seeds){
    const SimMisses clean = looked_for(rows, row);
    return Pokemon::run_row_seeds(row, Pokemon::story_seeds(seeds), [&](const std::string& id, uint64_t seed){
        return run_seed(rows, clean, id, seed);
    });
}


//  Row `row` for the seeds, under `floor` passes in a hundred a failure:
//  each row's floor is what it passed at this branch's tip, so a row that
//  passes less often has got worse. A row PA_BDSP_STORY_MC_ROWS leaves out
//  is skipped.
UnitTestResult test_row(Logger& logger, const std::string& row, size_t floor){
    if (Pokemon::selected_rows(ROWS_VARIABLE, {row}).empty()){
        return {UnitTestResult::SKIPPED, std::string(ROWS_VARIABLE) + " leaves it out"};
    }
    const size_t seeds = Pokemon::seed_count(SEEDS_VARIABLE, DEFAULT_SEEDS);
    const WallClock start = current_time();
    const RowReport report = run_row(story_checkpoints(), row, seeds);
    const auto took = std::chrono::duration_cast<Milliseconds>(current_time() - start);
    logger.log(Pokemon::format_report(report) + std::format("\n    ({} seeds in {} ms)", seeds, took.count()));
    //  The floors are the default seeds' counts; other counts only report.
    if (seeds == DEFAULT_SEEDS && report.passes * 100 < floor * report.runs){
        return std::format("{} passed {} of {}, under its floor of {}%:\n{}", row, report.passes, report.runs,
            floor, Pokemon::format_report(report));
    }
    return true;
}


//  01.03 as it was before be57db157 and 4f13c19bb: counted taps across
//  Roark's room, and the route to below the Center's door straight into
//  the heal.
std::vector<StoryCheckpoint> old_0103(){
    StoryStep taps;
    taps.kind = Pokemon::StoryStepKind::LEGS;
    taps.name = "above_roarks_row";
    taps.legs = {
        {.direction = Direction::EAST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 2},
        {.direction = Direction::WEST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 5},
        {.direction = Direction::EAST, .tiles = 14}, {.direction = Direction::SOUTH, .tiles = 10},
        {.direction = Direction::WEST, .tiles = 2}, {.direction = Direction::SOUTH, .tiles = 2},
    };
    taps.pose_after = Pokemon::tile_pose("OreburghMine", {TilePos{27, 28}}, Direction::SOUTH);
    const std::vector<std::string> added{"around_the_rock", "down_column_29",
        "south_of_the_centers_door", "against_the_walls_east_of_the_door", "below_the_centers_door_lined_up"};
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& row : rows){
        if (row.id != "01.03"){
            continue;
        }
        std::erase_if(row.steps, [&](const StoryStep& step){
            return std::find(added.begin(), added.end(), step.name) != added.end();
        });
        std::replace_if(row.steps.begin(), row.steps.end(), [](const StoryStep& step){ return step.name == "above_roarks_row"; }, taps);
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
        row.start = Pokemon::tile_pose("OreburghCity", {TilePos{45, 59}, TilePos{45, 58}}, Direction::NORTH);
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
//  Center door's column (10:39). The old row must show both.
UnitTestResult test_old_0103(Logger& logger){
    const size_t seeds = std::max<size_t>(200, Pokemon::seed_count(SEEDS_VARIABLE, DEFAULT_SEEDS));
    const RowReport whole = run_row(old_0103(), "01.03", seeds);
    logger.log("The old 01.03: " + Pokemon::format_report(whole));
    const RowReport town = run_row(from_the_mines_sign(old_0103()), "01.03", seeds);
    logger.log("The old 01.03 from the mine's sign: " + Pokemon::format_report(town));
    logger.log("Today's 01.03 from the mine's sign: " + Pokemon::format_report(run_row(from_the_mines_sign(story_checkpoints()), "01.03", seeds)));
    //  A battle on the taps, then the row ends off the tile the taps count.
    const bool roark = failed_with(whole, "01.03 above_roarks_row", "sim_mismatch", "[wild battle in the step]");
    const bool center = failed_with(town, "01.03 heal_oreburgh", "no door north");
    if (!roark || !center){
        return std::format("expected the old 01.03 to fail across Roark's room after a wild battle ({}) and at the Center's door ({}):\n{}\n{}",
            roark, center, Pokemon::format_report(whole), Pokemon::format_report(town));
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
    {.input = {"00.01"}, .expectation = {.floor = 20}},
    {.input = {"00.02"}, .expectation = {.floor = 100}},
    {.input = {"00.03"}, .expectation = {.floor = 30}},
    {.input = {"00.04"}, .expectation = {.floor = 0}},
    {.input = {"00.04b"}, .expectation = {.floor = 55}},
    {.input = {"00.04c"}, .expectation = {.floor = 100}},
    {.input = {"00.04d"}, .expectation = {.floor = 55}},
    {.input = {"00.05"}, .expectation = {.floor = 70}},
    {.input = {"00.05b"}, .expectation = {.floor = 45}},
    {.input = {"00.05c"}, .expectation = {.floor = 20}},
    {.input = {"01.01"}, .expectation = {.floor = 95}},
    {.input = {"01.02"}, .expectation = {.floor = 20}},
    {.input = {"01.02b"}, .expectation = {.floor = 90}},
    {.input = {"01.03"}, .expectation = {.floor = 5}},
    {.input = {"01.04"}, .expectation = {.floor = 100}},
    {.input = {"01.05"}, .expectation = {.floor = 80}},
    {.input = {"01.06"}, .expectation = {.floor = 25}},
    {.input = {"01.07"}, .expectation = {.floor = 100}},
    {.input = {"01.08"}, .expectation = {.floor = 40}},
    {.input = {"01.09"}, .expectation = {.floor = 50}},
    {.input = {"01.10"}, .expectation = {.floor = 5}},
    {.input = {"01.11"}, .expectation = {.floor = 100}},
    {
        .input = {"the old 01.03 fails across Roark's room and at the Center's door (lab-B, 2026-11-08)", test_old_0103},
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
