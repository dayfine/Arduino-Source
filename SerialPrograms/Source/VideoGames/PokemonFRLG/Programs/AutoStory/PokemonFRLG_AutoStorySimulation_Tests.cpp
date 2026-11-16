/*  AutoStory Simulation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run of the whole story table, new game to the Boulder Badge, without
 *  the console: story_checkpoints() built into engine checkpoints by the
 *  shared builder, saving at each end as the program does, run by the
 *  shared runner with the program's retry policy, on the simulation of the
 *  story maps (StoryMapsSim): the story's action table as the console's,
 *  bound to the simulation, which plans each ROUTE on the story maps from
 *  where the run's pose says we stand, walks each LEGS on them, steps onto
 *  triggers and stubs what needs the console, unless told to fail a step.
 *
 */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryPaceSim.h"
#include "PokemonFRLG_AutoStoryRngLoop.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryUnauthoredStarts_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::Outcome;
using Pokemon::RunState;

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n    " + line;
    }
    return text;
}

//  A checkpoint's start as the engine's Pose: nothing for a new game.
std::optional<Pokemon::Pose> start_pose(const StoryCheckpoint& checkpoint){
    return checkpoint.start ? std::optional<Pokemon::Pose>(Pokemon::pose_of(*checkpoint.start)) : std::nullopt;
}

//  The table's checkpoint `id`; throws if it has none.
const StoryCheckpoint& checkpoint_row(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [id](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint " + std::string(id) + ".");
    }
    return *row;
}

//  How long one test case may run, wall clock, before its run stops with
//  a failure that says so: CI's per-push run has no room for a sim that
//  went round in circles (#599's whole-story sims ran past 5 minutes each).
constexpr Seconds STAGE_BUDGET{120};
//  The whole story end to end, a slow test.
constexpr Seconds WHOLE_STORY_BUDGET{1200};

//  The running test case's deadline, set for its thread by
//  Test_AutoStorySimulation::run(): FakeGame::run() checks it before each
//  step.
thread_local std::optional<std::chrono::steady_clock::time_point> t_deadline;
thread_local Seconds t_budget{0};

//  The environment variable that turns on the slow tests: the whole story
//  end to end. Unset (Fork Draft CI's per-push run), they are SKIPPED;
//  the stage tests below cover the same rows.
constexpr std::string_view SLOW_TESTS_VARIABLE = "PA_SLOW_TESTS";

bool slow_tests_enabled(){
    // code-health: allow char_pointer -- std::getenv returns a const char pointer
    const char* value = std::getenv(std::string(SLOW_TESTS_VARIABLE).c_str());
    return value != nullptr && value[0] != '\0' && std::string_view(value) != "0";
}

//  The story on the story maps (StoryMapsSim), with a trace of what ran
//  and steps told to fail.
class FakeGame : public StoryMapsSim{
public:
    explicit FakeGame(StorySimKnobs knobs = {})
        : StoryMapsSim(std::move(knobs))
    {}

    struct Failure{
        std::string failure;
        size_t times = 0;
    };

    std::vector<std::string> trace;
    std::map<std::string, Failure> failures;
    //  Where the save loaded stands, when not at its checkpoint's start.
    std::optional<StoryPose> save_stands;

    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
        StoryMapsSim::begin_attempt(data);
        m_row = data.id;
        trace.push_back("begin " + data.id);
    }
    virtual Outcome run(const StoryStep& step, RunState& state) override{
        const std::string key = m_row + "/" + step.name;
        if (t_deadline && std::chrono::steady_clock::now() > *t_deadline){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format(
                "Time budget of {} s spent before {}: the run is stuck or too long for one test; split it.",
                t_budget.count(), key
            ));
        }
        trace.push_back(key);
        auto failure = failures.find(key);
        if (failure != failures.end() && failure->second.times > 0){
            failure->second.times--;
            return Pokemon::outcome_failed(failure->second.failure, key);
        }
        return StoryMapsSim::run(step, state);
    }

private:
    std::string m_row;
};


//  One simulated run, segment built as the program builds it: the rows a
//  run takes with every handoff off, each saving at its end (hooks.save
//  stands for save_game()).
struct Simulation{
    explicit Simulation(StorySimKnobs knobs = {})
        : game(std::move(knobs))
    {}

    std::vector<StoryCheckpoint> rows = plan_story_run(story_checkpoints(), 0, story_checkpoints().size() - 1, StoryHandoffs{}).rows;
    FakeGame game;
    std::vector<std::string> lines;
    //  Where each save stood: the end of its checkpoint.
    std::vector<std::pair<std::string, Pokemon::Pose>> saves;
    Pokemon::SegmentPolicy policy;

    Pokemon::SegmentReport run(const Pokemon::SegmentRange& range){
        Pokemon::Segment segment;
        segment.id = "00";
        for (const StoryCheckpoint& data : rows){
            segment.checkpoints.push_back(Pokemon::CheckpointSpec{
                .id = data.id, .title = data.title, .saves = true,
                .build = [this, &data]{
                    Pokemon::Checkpoint checkpoint = Pokemon::build_story_checkpoint(data, game);
                    add_start_pose_check(checkpoint, checked_start_pose(data), [this]{ return game.save_stands; }, [this](const std::string& line){ lines.push_back(line); });
                    return checkpoint;
                },
            });
        }
        Pokemon::SegmentHooks hooks;
        hooks.save = [this](RunState& state){
            game.trace.push_back("save");
            saves.emplace_back(last_begun(), state.pose);
            return Pokemon::outcome_ok();
        };
        hooks.reload = [this](RunState&){
            game.trace.push_back("reload");
            return Pokemon::outcome_ok();
        };
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        RunState state;
        return Pokemon::run_segment(range, policy, segment, hooks, state);
    }
    std::string last_begun() const{
        for (auto it = game.trace.rbegin(); it != game.trace.rend(); ++it){
            if (it->starts_with("begin ")){
                return it->substr(6);
            }
        }
        return "";
    }
    std::vector<std::string> begun() const{
        std::vector<std::string> ids;
        for (const std::string& item : game.trace){
            if (item.starts_with("begin ")){
                ids.push_back(item.substr(6));
            }
        }
        return ids;
    }
    size_t count(const std::string& item) const{
        return (size_t)std::count(game.trace.begin(), game.trace.end(), item);
    }
};


struct AutoStorySimulationInput{
    std::string name;
    UnitTestResult (*body)();
    //  A body that logs, run instead of `body` if given.
    UnitTestResult (*logged)(Logger&) = {};
    //  Its wall-clock budget.
    Seconds budget = STAGE_BUDGET;
    //  Run only with the slow tests on (SLOW_TESTS_VARIABLE).
    bool slow = false;
};

struct AutoStorySimulationExpectation{
    std::string_view description;
};

struct AutoStorySimulationTestCase{
    AutoStorySimulationInput input;
    AutoStorySimulationExpectation expectation;
};

class Test_AutoStorySimulation : public UnitTest{
public:
    explicit Test_AutoStorySimulation(AutoStorySimulationTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStory - Simulation: " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        if (m_scenario.input.slow && !slow_tests_enabled()){
            return UnitTestResult(UnitTestResult::SKIPPED, std::format("a slow test: set {}=1 to run it", SLOW_TESTS_VARIABLE));
        }
        t_budget = m_scenario.input.budget;
        t_deadline = std::chrono::steady_clock::now() + m_scenario.input.budget;
        try{
            UnitTestResult result = m_scenario.input.logged ? m_scenario.input.logged(logger) : m_scenario.input.body();
            t_deadline.reset();
            return result;
        }catch (Exception& e){
            t_deadline.reset();
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStorySimulationTestCase m_scenario;
};


//  The rows of a run, as Simulation::rows has them, from `first` to
//  `last`, both inclusive; all of them for an empty range.
std::vector<StoryCheckpoint> rows_in(const std::vector<StoryCheckpoint>& rows, const Pokemon::SegmentRange& range){
    std::vector<StoryCheckpoint> out;
    bool in = !range.from;
    for (const StoryCheckpoint& data : rows){
        in = in || data.id == *range.from;
        if (in){
            out.push_back(data);
        }
        if (range.to && data.id == *range.to){
            break;
        }
    }
    return out;
}

//  A run of `range` with nothing told to fail: through, each checkpoint
//  once in order and one-shot, each saving at its end, each starting where
//  the save before it stands; and 02.03's shopping, if it ran.
UnitTestResult check_story_run(const Pokemon::SegmentRange& range){
    Simulation sim;
    const Pokemon::SegmentReport report = sim.run(range);
    if (!report.completed){
        return "Expected the story through:" + joined(sim.lines);
    }
    const std::vector<StoryCheckpoint> rows = rows_in(sim.rows, range);
    std::vector<std::string> ids;
    for (const StoryCheckpoint& data : rows){
        ids.push_back(data.id);
    }
    if (sim.begun() != ids || sim.saves.size() != ids.size()){
        return "Expected each checkpoint once, in order, each saving at its end:" + joined(sim.game.trace);
    }
    //  Each checkpoint starts where the save before it stands, but one
    //  authored before the rows that lead up to it.
    for (size_t c = 1; c < rows.size(); c++){
        const StoryCheckpoint& data = rows[c];
        if (starts_after_unauthored_rows(data.id)){
            continue;
        }
        const std::optional<StoryPose> start = story_pose(*start_pose(data));
        const std::optional<StoryPose> saved = story_pose(sim.saves[c - 1].second);
        const bool same = start && saved && start->map == saved->map && start->tile == saved->tile
            && (!start->facing || start->facing == saved->facing);
        if (!same){
            return std::format("{} starts at {}, but {} saved at {}.", data.id,
                Pokemon::to_string(*start_pose(data)), sim.saves[c - 1].first, Pokemon::to_string(sim.saves[c - 1].second));
        }
    }
    if (report.one_shot_passes() != report.checkpoints_run){
        return "Expected every checkpoint one-shot:" + joined(sim.lines);
    }
    if (std::ranges::find(ids, "02.03") == ids.end()){
        return true;
    }
    //  02.03's shopping, with the cursor staying on each row bought.
    const std::map<std::string, int> bought{{"potion", 5}, {"antidote", 3}, {"parlyz_heal", 1}};
    if (sim.game.bag() != bought){
        std::string text;
        for (const auto& item : sim.game.bag()){
            text += std::format(" {} {}", item.second, item.first);
        }
        return "Expected 5 Potions, 3 Antidotes and 1 Parlyz Heal; bought" + text + ".";
    }
    return true;
}

UnitTestResult test_whole_story(){
    return check_story_run({});
}

//  The whole story through the table as the simulation binds it, each
//  row's stubs logged: a step is stubbed exactly when its action needs the
//  console, so no step the simulation plays is passed over.
UnitTestResult check_stubbed_steps(Logger& logger, const Pokemon::SegmentRange& range){
    Simulation sim;
    if (!sim.run(range).completed){
        return "Expected the story through:" + joined(sim.lines);
    }
    logger.log("Steps the simulation does not play, by row:\n" + stub_report(sim.game.stubs().rows()));
    for (const RowStubs& stubs : sim.game.stubs().rows()){
        const auto row = std::find_if(sim.rows.begin(), sim.rows.end(), [&](const StoryCheckpoint& data){ return data.id == stubs.row; });
        if (row == sim.rows.end()){
            return "Stubs for no row: " + stubs.row;
        }
        for (const StoryStep& step : row->steps){
            const Pokemon::StoryAction* action = sim.game.actions().find(step);
            const bool stubbed = std::ranges::find(stubs.steps, step.name) != stubs.steps.end();
            if (action == nullptr || stubbed != action->needs_console){
                return std::format("{} {}: {}, but its action {} the console.", row->id, step.name,
                    stubbed ? "stubbed" : "played", action != nullptr && action->needs_console ? "needs" : "does not need");
            }
        }
    }
    return true;
}

UnitTestResult test_start_and_end(){
    Simulation sim;
    const Pokemon::SegmentReport report = sim.run({.from = "02.01", .to = "02.03"});
    if (!report.completed || sim.begun() != std::vector<std::string>{"02.01", "02.02", "02.03"}){
        return "Expected 02.01 to 02.03 only:" + joined(sim.begun());
    }
    return true;
}

//  A walk that fails once (a trainer the walker could not recover from):
//  the checkpoint reloads its last save and runs again from its start.
UnitTestResult test_retry_from_save(){
    Simulation sim;
    sim.game.failures["03.01/forest_north"] = {.failure = "operation_failed", .times = 1};
    const Pokemon::SegmentReport report = sim.run({.from = "03.01", .to = "03.02"});
    if (!report.completed || report.reloads != 1 || sim.begun() != std::vector<std::string>{"03.01", "03.01", "03.02"}
        || sim.count("03.01/forest_east") != 2
    ){
        return "Expected 03.01 reloaded once and run again from its start:" + joined(sim.game.trace);
    }
    if (std::find(sim.lines.begin(), sim.lines.end(), "[Checkpoint] 03.01: passed on attempt 2 (1 reset)") == sim.lines.end()){
        return "Expected the retried pass logged:" + joined(sim.lines);
    }
    return true;
}

//  console B, FRLG 07.02, 2026-11-09: the save stood below Cerulean Center's
//  door (22, 19), and the run walked as if from the counter where 07.02
//  starts. It, and 03.02 from below Pewter Center's door (17, 25), stop
//  before any step or save.
UnitTestResult test_save_off_start_pose(){
    const std::pair<std::string, StoryPose> CASES[] = {
        {"03.02", StoryPose{"PewterCity", TilePos{17, 26}, Direction::SOUTH}},
        {"07.02", StoryPose{"CeruleanCity", TilePos{22, 20}, Direction::SOUTH}},
    };
    for (const auto& [id, save] : CASES){
        Simulation sim;
        sim.game.save_stands = save;
        const Pokemon::SegmentReport report = sim.run({.from = id, .to = id});
        const auto stepped = [&id](const std::string& item){ return item.starts_with(id + "/"); };
        const auto told = [](const std::string& line){ return line.starts_with("start pose mismatch: "); };
        if (report.completed || report.failure.failure != "start_failed" || !sim.saves.empty()
            || std::ranges::any_of(sim.game.trace, stepped) || std::ranges::none_of(sim.lines, told)
        ){
            return "Expected " + id + " stopped at its start check, the mismatch logged:" + joined(sim.game.trace) + joined(sim.lines);
        }
    }
    return true;
}

//  console B, FRLG 09.04, 2026-11-11: after a shiny Eevee from Gift RNG in
//  AutoStory, 09.04 stopped at its start check on the Eevee's summary,
//  which no save's start frame matches. The row after each RNG handoff
//  starts where the RNG program left the game, so it runs whatever the
//  screen shows.
UnitTestResult test_rows_after_rng_skip_start_check(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const auto index = [&checkpoints](std::string_view id){
        return (size_t)(&checkpoint_row(id) - checkpoints.data());
    };
    const StoryHandoffs handoffs{.starter_rng = true, .gift_rng = true};
    const std::vector<StoryCheckpoint> CASES[] = {
        rows_after_starter_rng(checkpoints, index("01.05"), index("01.06"), handoffs).rows,
        rows_after_gift_rng(checkpoints, index("04.02"), index("04.03"), handoffs).rows,
        rows_after_gift_rng(checkpoints, index("09.03"), index("09.04"), handoffs).rows,
    };
    for (const std::vector<StoryCheckpoint>& rows : CASES){
        Simulation sim;
        sim.rows = rows;
        sim.game.save_stands = StoryPose{"PewterCity", TilePos{17, 26}, Direction::SOUTH};
        const std::string& id = rows.front().id;
        const Pokemon::SegmentReport report = sim.run({.from = id, .to = id});
        const auto told = [](const std::string& line){ return line.starts_with("start pose mismatch: "); };
        if (!report.completed || std::ranges::any_of(sim.lines, told)){
            return std::format(
                "{} after its RNG handoff: expected it run with no start check; got {}{}{}",
                id, Pokemon::to_string(report.failure), joined(sim.game.trace), joined(sim.lines)
            );
        }
    }
    return true;
}

//  Brock's battle lost every time: three attempts, then the stop.
UnitTestResult test_gives_up_after_attempts(){
    Simulation sim;
    sim.game.failures["03.02/brock"] = {.failure = "blackout", .times = 10};
    const Pokemon::SegmentReport report = sim.run({.from = "03.02", .to = std::nullopt});
    if (report.completed || report.reloads != 2 || report.failed_checkpoint != "03.02" || report.failed_event != "brock"
        || report.failure.failure != "blackout" || sim.count("03.02/brock") != 3 || !sim.saves.empty()
    ){
        return "Expected 03.02 tried 3 times, then a stop on Brock:" + joined(sim.game.trace) + joined(sim.lines);
    }
    return true;
}

//  Walks `step` of 04.06 on `sim` from `at`, where the table says the
//  player stands, and moves `at` to where it says the step ends. Returns
//  the failure, or empty.
std::string walk_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step){
    const std::string where = "04.06/" + step.name + ": ";
    //  A walk that ends on an open leg may leave the player short of it.
    bool open_end = false;
    if (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS){
        const std::optional<std::vector<RouteStep>> route = walked_route(at, step);
        if (!route || route->empty()){
            return where + "no route from " + to_string(MapPosition{at.map, at.tile}) + ".";
        }
        const RoutePlanning planning{.maps = &story_maps(), .start = MapPosition{at.map, at.tile}, .label = step.name};
        const std::string failure = OverworldSimWalker(sim).run(*route, planning);
        if (!failure.empty()){
            return where + failure + " On " + to_string(sim.state().position) + ".";
        }
        const RouteStep& last = route->back();
        open_end = last.action == RouteAction::WALK && !last.to_wall && !last.warps;
        at = StoryPose{last.end.map, last.end.point, last.facing_after};
    }else if (step.kind == StoryStepKind::STEP){
        sim_step_once(sim, Pokemon::story_arg<Direction>(step.args, "direction"));
        sim.watch({SimWatcher::MESSAGE}, std::chrono::milliseconds(2000));
        at = *story_pose(Pokemon::pose_of(*step.pose_after));
    }
    if (!open_end && !(sim.state().position == MapPosition{at.map, at.tile})){
        return where + "expected " + to_string(MapPosition{at.map, at.tile}) + ", but on " + to_string(sim.state().position) + ".";
    }
    return "";
}

//  Console B, 2026-11-03 (row 04.06): the open leg east from Route 4
//  into Cerulean City ended a tile short, on (6, 20), with nothing on
//  screen. No hold to a wall finds the player there, and the walk went on
//  a column off, the hold south stopped by the fence on (6, 23), to a
//  wall below a row of houses, not the Pokemon Center's door. From the
//  foot of Route 4's ledge, the row's walks on the sim reach the nurse
//  with that leg whole or a tile short.
UnitTestResult test_cerulean_entry_short(){
    const StoryCheckpoint* row = &checkpoint_row("04.06");
    const auto hop = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "ledge_hop"; });
    if (hop == row->steps.end() || !hop->pose_after){
        return "04.06: no ledge_hop.";
    }
    for (int short_by : {0, 1}){
        StoryPose at = *story_pose(Pokemon::pose_of(*hop->pose_after));
        SimScript script;
        script.short_holds[1] = short_by;
        OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing, script);
        for (auto step = hop + 1; step != row->steps.end() && step->kind != StoryStepKind::HEAL; ++step){
            const std::string failure = walk_on_sim(sim, at, *step);
            if (!failure.empty()){
                return std::format("Entry {} tile(s) short: ", short_by) + failure;
            }
        }
    }
    return true;
}

//  Console B, 2026-11-03 (row 05.05), twice: the open leg north from
//  Cerulean City onto Nugget Bridge ended on (11, 33), a tile short of
//  (11, 32), and the one step onto Cale's row reached only (11, 32), out
//  of his sight: "bug_catcher_cale: failed: no_battle". From the walk's
//  end, whole or a tile short, the five trainers are met and beaten on
//  the sim, each battle on the trainer's row.
UnitTestResult test_nugget_bridge_short(){
    const StoryCheckpoint* row = &checkpoint_row("05.05");
    const auto walk = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "below_cale_line"; });
    if (walk == row->steps.end() || !walk->args.contains("target")){
        return "No step 05.05/below_cale_line.";
    }
    for (int short_by : {0, 1}){
        StoryPose at = *story_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(walk->args, "target")));
        OverworldSim sim(story_maps(), MapPosition{at.map, {at.tile.x, at.tile.y + short_by}}, Direction::NORTH);
        for (auto step = walk + 1; step != row->steps.end() && step->kind != StoryStepKind::READY; ++step){
            const std::string failure = row_step_on_sim(sim, at, *step, "05.05");
            if (!failure.empty()){
                return std::format("Walk {} tile(s) short: ", short_by) + failure;
            }
        }
        if (sim.state().trainers_beaten.size() != 5){
            return std::format("Walk {} tile(s) short: expected 5 trainers beaten, got {}.", short_by, sim.state().trainers_beaten.size());
        }
    }
    return true;
}

//  05.06 on the sim, from its start on Route 24 to the Sea Cottage:
//  Franklin, Kelsey, Nob and Haley are met on the steps into their lines
//  and beaten there. The sim's trainers who look around see every way
//  they may look, so Youngster Dan sees the player on (20, 4), walks up
//  to (21, 4) and is fought there, off the way on. Youngster Chad wanders
//  column 36 (rows 1..7, the map's wander box) and sees 2 tiles: every way
//  east crosses his column within his sight, so the sim, from his box's
//  tile (36, 7), sees the player on row 9 and fights him.
UnitTestResult test_route25_trainers(){
    const StoryCheckpoint* row = &checkpoint_row("05.06");
    StoryPose at = *story_pose(Pokemon::pose_of(*row->start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row->steps){
        const std::string failure = row_step_on_sim(sim, at, step, "05.06");
        if (!failure.empty()){
            return failure;
        }
    }
    const std::vector<std::string> expected{"hiker", "youngster", "picnicker", "hiker", "youngster", "lass"};
    if (sim.state().trainers_beaten != expected){
        std::string beaten;
        for (const std::string& name : sim.state().trainers_beaten){
            beaten += " " + name;
        }
        return "05.06: expected Franklin, Dan, Kelsey, Nob, Chad and Haley beaten; beat" + beaten + ".";
    }
    if (!(sim.state().position == MapPosition{"Route25_SeaCottage", {7, 8}})){
        return "05.06: expected to end in the Sea Cottage on (7, 8), not " + to_string(sim.state().position) + ".";
    }
    return true;
}

//  05.09 on the sim, its walks from the Center: over Nugget Bridge, whose
//  trainers 05.05 beat (they stand there, but don't battle), onto Route
//  24's grass at (4, 19) out of Camper Shane's sight, and back to the
//  nurse. No trainer battles on the way. The open leg north from Cerulean
//  City onto the bridge crosses onto Route 24, where the game holds the
//  player up a tile: held for its tiles alone it ended on (11, 16), a tile
//  short, and the leg west bumped the bridge's posts (console B,
//  2026-11-03, row 05.09, twice).
//  The story maps after 05.05: Nugget Bridge's trainers beaten, standing
//  on columns 10 and 12 without battling.
std::vector<MapData> maps_bridge_beaten(){
    std::vector<MapData> maps = story_maps();
    for (MapData& map : maps){
        if (map.name != "Route24"){
            continue;
        }
        for (MapObject& object : map.objects){
            if (object.trainer && object.x >= 10 && object.x <= 12){
                object.trainer.reset();
            }
        }
    }
    return maps;
}

UnitTestResult test_oddish_walks(){
    const StoryCheckpoint* row = &checkpoint_row("05.09");
    StoryPose at = *story_pose(Pokemon::pose_of(*row->start));
    OverworldSim sim(maps_bridge_beaten(), MapPosition{at.map, at.tile}, *at.facing);
    const std::map<std::string, MapPosition> ends{
        {"mart_clerk", {"CeruleanCity_Mart", {4, 3}}},
        {"oddish_patch", {"Route24", {3, 19}}},
        {"oddish_patch_cliff", {"Route24", {3, 19}}},
        {"cerulean_nurse", {"CeruleanCity_PokemonCenter_1F", {7, 4}}},
    };
    for (const StoryStep& step : row->steps){
        if (step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS){
            continue;
        }
        const std::string failure = row_step_on_sim(sim, at, step, "05.09");
        if (!failure.empty()){
            return failure;
        }
        const auto end = ends.find(step.name);
        if (end == ends.end() || !(sim.state().position == end->second)){
            return "05.09/" + step.name + ": ended on " + to_string(sim.state().position) + ".";
        }
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "05.09: a trainer's battle or a violation on the way.";
    }
    return true;
}

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

//  Walks `route`, planned from the pacing step's tiles by
//  plan_story_route(), on `sim`, as walk_route() does: no start tile, the
//  facing read. Returns the failure, or empty.
std::string walk_paced_route(OverworldSim& sim, const std::vector<RouteStep>& route, const std::string& label){
    const RoutePlanning planning{.maps = &story_maps(), .start = std::nullopt, .label = label};
    const std::string failure = OverworldSimWalker(sim).run(route, planning);
    return failure.empty() ? "" : label + ": " + failure + " On " + to_string(sim.state().position) + ".";
}

//  A ROUTE's target as a pose, or nothing for another step.
std::optional<Pokemon::Pose> target_of(const StoryStep& step){
    const Pokemon::GridPose* target = Pokemon::value_if_at_it<Pokemon::GridPose>(step.args, step.args.find("target"));
    return target == nullptr ? std::nullopt : std::optional<Pokemon::Pose>(Pokemon::pose_of(*target));
}

//  Console B, 2026-11-03 (row 05.09, run m61): the Oddish's battle came
//  on the pace's far step, so the capture left the player on (3, 19), not
//  the table's (4, 19). The walk back planned from (4, 19) went a column
//  west of its plan, down the bridge's column 10, not 11, and stopped
//  against the beaten Lass on (10, 22); every later leg bumped, and the
//  warp to the Center never came. On the sim, the battle on either step
//  of the pace, the row's later walks, from the capture's tiles, reach the
//  nurse.
//  Console B, 2026-11-03 (m63, 05.09 verify run 2): the open leg west
//  from Nugget Bridge's head ended a tile short, so the walk down onto the
//  grass ended on (5, 19), in Camper Shane's sight; after the first wild
//  battle he walked up ("I saw your feat from the grass!") and the capture
//  stopped. From the Mart, the walk to the patch on the sim keeps out of
//  his sight with the leg west whole or a tile short.
UnitTestResult test_oddish_patch_west_short(){
    const StoryCheckpoint* row = &checkpoint_row("05.09");
    const auto mart = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "mart_clerk"; });
    const auto patch = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "oddish_patch"; });
    if (mart == row->steps.end() || patch == row->steps.end() || !target_of(*mart) || !target_of(*patch)){
        return "05.09: no mart_clerk or oddish_patch.";
    }
    for (int short_by : {0, 1}){
        StoryPose at = *story_pose(*target_of(*mart));
        SimScript script;
        //  Holds from the Mart: south, west, north, east, north, east, north
        //  over the bridge, then the 8th, west from the bridge's head.
        script.short_holds[8] = short_by;
        OverworldSim sim(maps_bridge_beaten(), MapPosition{at.map, at.tile}, *at.facing, script);
        const std::string failure = row_step_on_sim(sim, at, *patch, "05.09");
        const std::string label = std::format("West leg {} tile(s) short: ", short_by);
        if (!failure.empty()){
            return label + failure;
        }
        if (!sim.state().trainers_beaten.empty()){
            return label + "a trainer saw the player, on " + to_string(sim.state().position) + ".";
        }
        const StoryPose end = *story_pose(*target_of(*patch));
        if (!(sim.state().position == MapPosition{end.map, end.tile})){
            return label + "ended on " + to_string(sim.state().position) + ", not " + to_string(MapPosition{end.map, end.tile}) + ".";
        }
    }
    return true;
}

UnitTestResult test_oddish_battle_on_far_step(){
    const StoryCheckpoint* row = &checkpoint_row("05.09");
    const auto capture = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "catch_oddish"; });
    if (capture == row->steps.end() || capture + 1 == row->steps.end() || !target_of(*(capture + 1))){
        return "05.09: no catch_oddish with a route after it.";
    }
    const auto patch = std::find_if(row->steps.begin(), capture, [](const StoryStep& step){ return step.name == "oddish_patch"; });
    if (patch == capture || !target_of(*patch)){
        return "05.09: no oddish_patch before catch_oddish.";
    }
    //  The sim starts on the patch (3, 19): grass step 1 is the pace's far
    //  step onto (4, 19), step 2 the one back.
    for (int grass_step : {1, 2}){
        SimScript script;
        script.encounters.push_back(SimEncounter{.grass_step = grass_step});
        const StoryPose start = *story_pose(*target_of(*patch));
        OverworldSim sim(maps_bridge_beaten(), MapPosition{start.map, start.tile}, Direction::SOUTH, script);
        const std::string label = std::format("Battle on grass step {}: ", grass_step);
        QuietLogger logger;
        SimPatch pacing(sim, Direction::EAST, Direction::WEST);
        if (!pace_until_battle(logger, pacing, PACE_MAX_STEPS) || sim.handle_battle(SimBattleChoice::RUN) != SimBattleEnd::ENDED){
            return label + "no wild battle on the pace.";
        }
        const StoryStep& back = *(capture + 1);
        StoryPose at = *story_pose(*target_of(back));
        const std::optional<std::vector<RouteStep>> route = plan_story_route(
            story_maps(), paced_pose(*target_of(*patch), *capture), std::nullopt, RouteGoal{{at.map, at.tile}, at.facing}
        );
        if (!route){
            return label + "no route from the capture's tiles.";
        }
        if (const std::string failure = walk_paced_route(sim, *route, "05.09/" + back.name); !failure.empty()){
            return label + failure;
        }
        for (auto step = capture + 2; step != row->steps.end(); ++step){
            const std::string failure = row_step_on_sim(sim, at, *step, "05.09");
            if (!failure.empty()){
                return label + failure;
            }
        }
        if (!(sim.state().position == MapPosition{"CeruleanCity_PokemonCenter_1F", {7, 4}})){
            return label + "05.09 ended on " + to_string(sim.state().position) + ", not the nurse.";
        }
    }
    return true;
}

//  Whether `row`'s step `c` is a GRIND or capture that paces, with a ROUTE
//  after it: nothing if not; else the route, planned from the pace's two
//  tiles (paced_pose()), walked on the sim from each: the failure, or empty.
std::optional<std::string> route_after_pacing(const StoryCheckpoint& row, size_t c){
    if (c == 0 || c + 1 >= row.steps.size() || row.steps[c + 1].kind != StoryStepKind::ROUTE || !target_of(row.steps[c - 1])){
        return std::nullopt;
    }
    const StoryStep& next = row.steps[c + 1];
    const Pokemon::Pose paced = paced_pose(*target_of(row.steps[c - 1]), row.steps[c]);
    const std::vector<TilePos> tiles = Pokemon::tiles_of(paced);
    if (tiles.size() < 2){
        return std::nullopt;
    }
    const StoryPose end = *story_pose(*target_of(next));
    const std::optional<std::vector<RouteStep>> route = plan_story_route(
        story_maps(), paced, std::nullopt, RouteGoal{{end.map, end.tile}, end.facing}
    );
    const std::string where = row.id + "/" + next.name;
    if (!route){
        return where + ": no route from " + Pokemon::to_string(paced) + ".";
    }
    for (const TilePos& tile : tiles){
        const std::string from = "From " + to_string(MapPosition{paced.map, tile}) + ": ";
        OverworldSim sim(maps_bridge_beaten(), MapPosition{paced.map, tile}, Direction::SOUTH);
        if (const std::string failure = walk_paced_route(sim, *route, where); !failure.empty()){
            return from + failure;
        }
        if (!(sim.state().position == MapPosition{end.map, end.tile})){
            return from + where + " ended on " + to_string(sim.state().position) + ".";
        }
    }
    return std::string();
}

//  Every GRIND or capture that paces, and a ROUTE after it: the route
//  planned from the pace's two tiles walks on the sim from either to its
//  end, its first holds bringing them to one tile.
UnitTestResult test_routes_after_pacing(){
    size_t checked = 0;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (size_t c = 0; c < row.steps.size(); c++){
            const std::optional<std::string> failure = route_after_pacing(row, c);
            if (failure && !failure->empty()){
                return *failure;
            }
            checked += failure ? 1 : 0;
        }
    }
    if (checked < 3){
        return std::format("Expected the routes after 05.01's and 05.03's GRINDs and 05.09's capture; checked {}.", checked);
    }
    return true;
}

//  04.06 on the sim, from Mt. Moon's exit (32, 6): east and up through
//  (48, 5) into the tutors' pen on Route 4 to (50, 4), facing the Mega
//  Kick tutor (50, 3), then on to above the ledge (93, 8). Each walk ends
//  where the table says, and no trainer sees the player on the way.
UnitTestResult test_mega_kick_tutor_walks(){
    const StoryCheckpoint* row = &checkpoint_row("04.06");
    const auto ledge = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "above_ledge"; });
    if (ledge == row->steps.end()){
        return "04.06: no above_ledge.";
    }
    StoryPose at = *story_pose(Pokemon::pose_of(*row->start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    bool at_tutor = false;
    for (auto step = row->steps.begin(); step != ledge + 1; ++step){
        if (step->kind != StoryStepKind::ROUTE && step->kind != StoryStepKind::LEGS){
            continue;
        }
        const std::string failure = row_step_on_sim(sim, at, *step, "04.06");
        if (!failure.empty()){
            return failure;
        }
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return "04.06/" + step->name + ": expected " + to_string(MapPosition{at.map, at.tile}) + ", but on " + to_string(sim.state().position) + ".";
        }
        if (sim.state().position == MapPosition{"Route4", {50, 4}} && sim.state().facing == Direction::NORTH){
            at_tutor = true;
        }
    }
    if (!at_tutor){
        return "04.06: no walk ended on Route 4's (50, 4) facing the Mega Kick tutor (50, 3).";
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "04.06: a trainer's battle or a violation on the way.";
    }
    return true;
}

//  Console B, 2026-11-03 (row 05.07): after his scene Bill stands on
//  (7, 6), east of the player on (6, 6), not on the maps' (7, 5). The
//  walk out planned east onto (7, 6) and south to the mat (7, 9); the
//  east tap bumped Bill, and the hold south took the player down column 6
//  to (6, 9), beside the mat, with no warp. On a copy of the maps with
//  Bill on (7, 6) and only the mat (7, 9) as the way out (the console
//  didn't warp from (6, 9)), the row's walks after the ticket end each
//  where the table says and leave on Route 25's (51, 5).
UnitTestResult test_out_of_bill_cottage(){
    const StoryCheckpoint* row = &checkpoint_row("05.07");
    const auto ticket = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "ss_ticket"; });
    if (ticket == row->steps.end() || !ticket->pose_after){
        return "05.07: no ss_ticket.";
    }
    std::vector<MapData> maps = story_maps();
    for (MapData& map : maps){
        if (map.name != "Route25_SeaCottage"){
            continue;
        }
        for (MapObject& object : map.objects){
            if (object.name == "bill"){
                object.x = 7;
                object.y = 6;
            }
        }
        std::erase_if(map.warps, [](const MapWarp& warp){ return warp.x != 7; });
    }
    StoryPose at = *story_pose(Pokemon::pose_of(*ticket->pose_after));
    OverworldSim sim(maps, MapPosition{at.map, at.tile}, *at.facing);
    for (auto step = ticket + 1; step != row->steps.end(); ++step){
        if (step->kind != StoryStepKind::ROUTE && step->kind != StoryStepKind::LEGS){
            continue;
        }
        const std::string failure = row_step_on_sim(sim, at, *step, "05.07");
        if (!failure.empty()){
            return failure;
        }
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return "05.07/" + step->name + ": expected " + to_string(MapPosition{at.map, at.tile}) + ", but on " + to_string(sim.state().position) + ".";
        }
    }
    if (!(sim.state().position == MapPosition{"Route25", {51, 5}})){
        return "05.07: expected to leave onto Route 25's (51, 5), not " + to_string(sim.state().position) + ".";
    }
    if (!sim.state().violations.empty()){
        return "05.07: violations:" + joined(sim.state().violations);
    }
    return true;
}


//  Someone talks to the player before every step (StorySimKnobs::
//  dialog_before): cleared between steps, the whole story still passes;
//  left up, as before the executor looked, a walk fails on it.
UnitTestResult check_dialog_before_every_step(const Pokemon::SegmentRange& range){
    Simulation cleared({.dialog_before = [](const std::string&, const std::string&){ return true; }});
    if (!cleared.run(range).completed){
        return "Expected the story through, a dialog before every step cleared:" + joined(cleared.lines);
    }
    return true;
}

//  The same dialogs left up, as before the executor looked: the story's
//  first walk fails on one.
UnitTestResult test_dialog_left_up_stops_a_walk(){
    const auto always = [](const std::string&, const std::string&){
        return true;
    };
    Simulation left({.dialog_before = always, .clear_interrupts = false});
    const bool completed = left.run({}).completed;
    const auto stopped = [](const std::string& line){
        return line.find("is still up") != std::string::npos;
    };
    if (completed || std::ranges::none_of(left.lines, stopped)){
        return "Expected a dialog left up to stop a walk:" + joined(left.lines);
    }
    return true;
}

//  Dialogs pop up at random before one step in five: how many whole runs
//  pass with them cleared between steps, and with them left up.
UnitTestResult check_dialogs_at_random(Logger& logger, const Pokemon::SegmentRange& range){
    constexpr uint32_t SEEDS = 5;
    size_t passes[2] = {0, 0};
    for (uint32_t seed = 1; seed <= SEEDS; seed++){
        for (bool clear : {false, true}){
            const auto rng = std::make_shared<std::mt19937>(seed);
            Simulation sim({
                .dialog_before = [rng](const std::string&, const std::string&){
                    return std::bernoulli_distribution(0.2)(*rng);
                },
                .clear_interrupts = clear,
            });
            passes[clear] += sim.run(range).completed ? 1 : 0;
        }
    }
    logger.log(std::format(
        "Dialogs before one step in five, {} to {}, {} seeds: {} passed left up, {} cleared between steps.",
        range.from.value_or("the start"), range.to.value_or("the end"), SEEDS, passes[0], passes[1]
    ));
    if (passes[1] != SEEDS){
        return std::format("Expected every run through with the dialogs cleared, {} of {}.", passes[1], SEEDS);
    }
    return true;
}


//  No ANSWER step follows a ROUTE or LEGS step. A walk closes a message
//  that shows at its end with B (MessageInterrupt), which answers a YES/NO
//  NO by itself, and the ANSWER after it then finds none. Console B,
//  2026-11-14, 14.02b: Bill's scene ran as below_gym_door's warp out of
//  Cinnabar Gym landed, the walk pressed through his question, and
//  decline_bill failed with "answer_yes_no(): no YES/NO". A question that
//  shows on arrival follows a STEP onto the warp, which leaves it up.
UnitTestResult test_no_answer_after_walk(){
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (size_t c = 1; c < row.steps.size(); c++){
            const StoryStep& walk = row.steps[c - 1];
            const StoryStep& answer = row.steps[c];
            if (answer.kind != StoryStepKind::ANSWER){
                continue;
            }
            if (walk.kind == StoryStepKind::ROUTE || walk.kind == StoryStepKind::LEGS){
                return std::format("{}/{}: follows the walk {}, which answers a YES/NO at its end with B.", row.id, answer.name, walk.name);
            }
        }
    }
    return true;
}


//  The `legs` events in the story before 2026-11-15, row/step. The user
//  (2026-11-15, 17.03's hand-written legs: "not ok"): every walk is a
//  planned `route` to a target tile, the gym's spinners planner moves too;
//  these are listed, not rewritten, and leave the list as they become
//  routes.
const std::set<std::string, std::less<>>& legs_before_2026_11_15(){
    static const std::set<std::string, std::less<>> LEGS{
        "03.01/forest_east", "03.01/forest_exit", "04.01/below_colton_line", "04.01/past_ben",
        "04.01/above_james_line", "04.01/save_spot", "04.05/mt_moon_1f", "04.05/b2f_south",
        "04.05/below_miguel_trigger", "04.05/below_helix_fossil", "04.06/above_city_ledge",
        "05.01/gym_west_walkway", "05.01/below_misty_platform", "05.02/gym_way_out", "05.05/below_ali_line",
        "05.05/below_timmy_line", "05.05/below_reli_line", "05.05/below_ethan_line", "05.06/west_of_dan",
        "05.06/round_dan", "05.06/west_of_nob_line", "05.06/below_haley_line", "05.07/beside_bill",
        "05.07/around_bill", "05.10/round_keigo_and_elijah", "06.01/past_the_cabins", "06.01/round_the_sailor",
        "06.02/west_of_dwayne_line", "06.02/east_of_baily_line", "06.02/below_tucker_line",
        "06.03/round_isabelle_line", "06.03/down_to_jeff_row", "07.01/round_isabelle_and_elijah",
        "07.02/round_brice", "07.02/above_caitlin_line", "07.02/below_center_door", "08.01/above_ashton_line",
        "08.01/winston_line_end", "08.01/below_martha_line", "08.01/below_allen_line", "08.01/west_of_b1f_ladder",
        "08.01/above_lenny_line", "08.01/west_of_lucas_line", "08.01/east_of_sofia_line",
        "08.01/east_of_dudley_line", "08.01/past_cooper", "08.01/above_dana_line", "08.01/above_south_mouth",
        "08.02/route10_east_side", "08.02/below_center_door", "08.03/lavender_west_edge",
        "08.03/east_of_rich_line", "08.03/west_of_glenn_column", "08.03/route8_row7_west_wall",
        "08.03/route8_row14_west_wall", "08.03/east_of_underground_door", "09.01/b2f_to_stairs",
        "09.01/b3f_arrow_maze", "09.01/beside_lift_key", "09.01/b4f_east_to_wall", "09.01/b4f_to_stairs",
        "09.01/b3f_down_to_row_20", "09.01/b3f_row_20_to_wall", "09.01/b3f_back_to_candy_row",
        "09.02/b3f_to_column_12", "09.02/b3f_to_top_corridor", "09.02/b2f_arrows_to_moon_stone",
        "09.02/beside_moon_stone", "09.02/b2f_arrows_to_lift_room", "09.02/lift_room_to_row_17",
        "09.02/row_17_to_wall", "09.02/below_lift", "09.02/below_left_guard", "09.02/below_right_guard",
        "09.02/west_of_right_guard", "09.02/up_column_17", "09.02/beside_giovanni", "10.04/tower_3f_to_stairs",
        "10.04/tower_4f_column_16", "10.04/tower_5f_row6_east", "10.05/tower_6f_below_jennifer_line",
        "10.05/above_tower_rare_candy", "10.05/above_marowak_trigger", "10.06/below_fuji_house",
        "11.02/top_of_cycling_road", "11.02/route18_west_wall", "11.03/before_east_grass",
        "11.03/to_full_restore", "11.03/to_north_area", "11.03/to_west_area", "11.03/below_gold_teeth",
        "11.03/below_secret_house_attendant", "12.01/center_northwest_corner", "12.01/below_kayden_line",
        "12.01/east_column_to_kirk", "12.01/east_column_to_wall", "12.01/top_row_to_wall", "12.01/down_to_koga",
        "12.02b/beside_warden", "13.01/statue_row", "13.01/statue_wall", "13.01/statue_tile",
        "14.01/center_northwest_corner", "14.01/below_quiz_1", "14.01/below_door_1", "14.01/to_quiz_2_machine",
        "14.01/below_quiz_2", "14.01/above_door_2", "14.01/below_quiz_3", "14.01/above_door_3",
        "14.01/below_quiz_4", "14.01/east_of_door_4", "14.01/below_quiz_5", "14.01/below_door_5",
        "14.01/below_quiz_6", "14.01/below_door_6", "14.01/below_blaine", "14.02b/above_door_6",
        "14.02b/above_door_5", "14.02b/west_of_door_4", "14.02b/below_door_3", "14.02b/below_door_2",
        "14.02b/above_door_1", "14.03/below_gym_gap", "14.03/column_1_west", "14.03/column_1_to_wall",
        "14.03/beside_inner_tree", "14.03/beside_erika", "14.04b/below_lori", "14.04b/row_5_to_wall",
        "14.04b/column_1_down_to_wall", "14.04b/onto_column_2", "14.04b/column_2_to_wall",
        "16.02/to_card_key_ball",
    };
    return LEGS;
}

//  What a new `legs` event's why says, then why the planner can't walk it
//  (taps beside a wall where a run overshoots, as 12.02b's beside_warden).
constexpr std::string_view NOT_PLANNED = "not planned: ";

//  No new `legs` event without a reason the planner can't walk it, and
//  every listed one still in the story.
UnitTestResult test_legs_need_a_reason(){
    std::set<std::string, std::less<>> listed_seen;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.kind != StoryStepKind::LEGS){
                continue;
            }
            const std::string key = row.id + "/" + step.name;
            if (legs_before_2026_11_15().contains(key)){
                listed_seen.insert(key);
                continue;
            }
            if (step.why.find(NOT_PLANNED) == std::string::npos){
                return std::format("{}: a legs event; make it a planned route to its target tile, or start its why's reason with \"{}\".", key, NOT_PLANNED);
            }
        }
    }
    for (const std::string& key : legs_before_2026_11_15()){
        if (!listed_seen.contains(key)){
            return std::format("{} is no legs event now: take it off legs_before_2026_11_15().", key);
        }
    }
    return true;
}

//  A stage of the story: its first and last checkpoints.
struct StoryStage{
    std::string first;
    std::string last;
};

//  The story's stages, as a run with every handoff off takes its rows:
//  each run of rows whose ids share a stage number ("05" for 05.01 to
//  05.10).
std::vector<StoryStage> story_stages(){
    const std::vector<StoryCheckpoint> rows = plan_story_run(story_checkpoints(), 0, story_checkpoints().size() - 1, StoryHandoffs{}).rows;
    std::vector<StoryStage> stages;
    for (const StoryCheckpoint& data : rows){
        if (stages.empty() || stages.back().last.substr(0, 2) != data.id.substr(0, 2)){
            stages.push_back({.first = data.id, .last = data.id});
        }else{
            stages.back().last = data.id;
        }
    }
    return stages;
}

//  Without a sim: each checkpoint starts where the one before it declares
//  its end (checkpoint_end_pose()), so the stage tests, each from its own
//  first row's start, chain into the whole story. A row whose end is left
//  to its legs' taps (04.01, 05.01) is checked by its stage's run; a
//  stage's last row must declare its end.
UnitTestResult test_declared_ends_chain(){
    const std::vector<StoryCheckpoint> rows = plan_story_run(story_checkpoints(), 0, story_checkpoints().size() - 1, StoryHandoffs{}).rows;
    for (size_t c = 1; c < rows.size(); c++){
        const StoryCheckpoint& before = rows[c - 1];
        const StoryCheckpoint& data = rows[c];
        if (starts_after_unauthored_rows(data.id)){
            continue;
        }
        const std::optional<Pokemon::GridPose> end = Pokemon::checkpoint_end_pose(before);
        if (!end){
            if (before.id.substr(0, 2) != data.id.substr(0, 2)){
                return std::format("{} ends its stage, but declares no end for {} to start from.", before.id, data.id);
            }
            continue;
        }
        const std::optional<StoryPose> ends = story_pose(Pokemon::pose_of(*end));
        const std::optional<StoryPose> start = data.start ? story_pose(Pokemon::pose_of(*data.start)) : std::nullopt;
        const bool same = ends && start && start->map == ends->map && start->tile == ends->tile
            && (!start->facing || !ends->facing || start->facing == ends->facing);
        if (!same){
            return std::format("{} starts at {}, but {} declares its end at {}.", data.id,
                data.start ? Pokemon::to_string(*start_pose(data)) : std::string("a new game"), before.id, Pokemon::to_string(Pokemon::pose_of(*end)));
        }
    }
    return true;
}

//  What a stage test runs on its rows.
enum class StageCheck{
    //  check_story_run()
    IN_ORDER,
    //  check_stubbed_steps()
    STUBS,
    //  check_dialog_before_every_step()
    DIALOG_EVERY_STEP,
    //  check_dialogs_at_random()
    DIALOGS_AT_RANDOM,
};

struct AutoStoryStageInput{
    StageCheck check;
    StoryStage stage;
};

struct AutoStoryStageCase{
    std::string name;
    AutoStoryStageInput input;
    AutoStorySimulationExpectation expectation;
};

//  One stage of the story, from its first row's start (as a save of the
//  row before it leaves the game: test_declared_ends_chain()) to its last
//  row's end, within STAGE_BUDGET.
class Test_AutoStoryStage : public UnitTest{
public:
    explicit Test_AutoStoryStage(AutoStoryStageCase scenario)
        : UnitTest("PokemonFRLG::AutoStory - Simulation: " + scenario.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        t_budget = STAGE_BUDGET;
        t_deadline = std::chrono::steady_clock::now() + STAGE_BUDGET;
        try{
            UnitTestResult result = run_check(logger);
            t_deadline.reset();
            return result;
        }catch (Exception& e){
            t_deadline.reset();
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    UnitTestResult run_check(Logger& logger) const{
        const Pokemon::SegmentRange stage{.from = m_scenario.input.stage.first, .to = m_scenario.input.stage.last};
        switch (m_scenario.input.check){
        case StageCheck::IN_ORDER:
            return check_story_run(stage);
        case StageCheck::STUBS:
            return check_stubbed_steps(logger, stage);
        case StageCheck::DIALOG_EVERY_STEP:
            return check_dialog_before_every_step(stage);
        case StageCheck::DIALOGS_AT_RANDOM:
            return check_dialogs_at_random(logger, stage);
        }
        return "Unknown stage check.";
    }

    AutoStoryStageCase m_scenario;
};

std::vector<AutoStoryStageCase> stage_cases(){
    struct Check{
        StageCheck check;
        std::string_view what;
        std::string_view description;
    };
    const Check CHECKS[] = {
        {StageCheck::IN_ORDER, "in order, each start where the last save stood", "runs the stage's checkpoints one-shot, each from where the one before saved"},
        {StageCheck::STUBS, "the steps it stubs need the console", "stubs each needs-console step of the stage and plays the rest"},
        {StageCheck::DIALOG_EVERY_STEP, "a dialog before every step, cleared", "the stage passes with each dialog cleared between steps"},
        {StageCheck::DIALOGS_AT_RANDOM, "dialogs before one step in five, over 5 seeds", "every run of the stage passes with them cleared"},
    };
    std::vector<AutoStoryStageCase> cases;
    for (const StoryStage& stage : story_stages()){
        for (const Check& check : CHECKS){
            cases.push_back({
                .name = std::format("stage {} ({} to {}), {}", stage.first.substr(0, 2), stage.first, stage.last, check.what),
                .input = {.check = check.check, .stage = stage},
                .expectation = {.description = check.description},
            });
        }
    }
    return cases;
}


const AutoStorySimulationTestCase AUTO_STORY_SIMULATION_CASES[] = {
    {.input = {.name = "new game to Brock, in order, each start where the last save stood", .body = test_whole_story, .budget = WHOLE_STORY_BUDGET, .slow = true}, .expectation = {.description = "runs the whole story end to end; slow, the stage tests run it on every push"}},
    {.input = {.name = "each checkpoint starts where the one before declares its end", .body = test_declared_ends_chain}, .expectation = {.description = "the stages chain into the whole story, no sim"}},
    {.input = {.name = "start and end checkpoints", .body = test_start_and_end}, .expectation = {.description = "executes segment bounded by start and end checkpoints"}},
    {.input = {.name = "a failed walk reloads its checkpoint's save", .body = test_retry_from_save}, .expectation = {.description = "reloads checkpoint save state on simulated movement failure"}},
    {.input = {.name = "a battle lost every time stops after 3 attempts", .body = test_gives_up_after_attempts}, .expectation = {.description = "aborts after three consecutive battle blackouts"}},
    {.input = {.name = "04.06 into Cerulean, the entry leg a tile short", .body = test_cerulean_entry_short}, .expectation = {.description = "recovers when entering Cerulean City if movement ends one tile short"}},
    {.input = {.name = "04.06 from Mt. Moon to the Mega Kick tutor, then above the ledge", .body = test_mega_kick_tutor_walks}, .expectation = {.description = "verifies route navigation from Mt. Moon to tutor and past ledge"}},
    {.input = {.name = "05.05 Nugget Bridge, the walk onto it a tile short", .body = test_nugget_bridge_short}, .expectation = {.description = "completes Nugget Bridge battles even if initial approach stops short"}},
    {.input = {.name = "05.06 Route 25, its four trainers and Dan", .body = test_route25_trainers}, .expectation = {.description = "simulates Route 25 trainer battles and Dan detour"}},
    {.input = {.name = "05.07 out of Bill's Sea Cottage, Bill on (7, 6)", .body = test_out_of_bill_cottage}, .expectation = {.description = "simulates exiting Bill sea cottage with Bill relocated"}},
    {.input = {.name = "05.09 to Route 24's grass and back, past Shane", .body = test_oddish_walks}, .expectation = {.description = "navigates to Route 24 grass avoiding trainer line of sight"}},
    {.input = {.name = "05.09 back to the Center, the Oddish's battle on the pace's far step", .body = test_oddish_battle_on_far_step}, .expectation = {.description = "handles Oddish capture encounter occurring on far pace step"}},
    {.input = {.name = "05.09 onto the grass out of Shane's sight, the leg west a tile short", .body = test_oddish_patch_west_short}, .expectation = {.description = "recovers when approaching grass patch with short western leg"}},
    {.input = {.name = "the routes after pacing, from either of its tiles", .body = test_routes_after_pacing}, .expectation = {.description = "plans valid routes regardless of which pacing tile character ends on"}},
    {.input = {.name = "a save off the start pose stops before any step", .body = test_save_off_start_pose}, .expectation = {.description = "fails the start check without walking or saving"}},
    {.input = {.name = "the row after an RNG handoff runs off any start pose", .body = test_rows_after_rng_skip_start_check}, .expectation = {.description = "01.06, 04.03 and 09.04 start where the RNG program left the game"}},
    {.input = {.name = "a dialog left up before every step stops a walk", .body = test_dialog_left_up_stops_a_walk}, .expectation = {.description = "left up, as before the executor looked, a walk fails on it"}},
    {.input = {.name = "an ANSWER never follows a walk", .body = test_no_answer_after_walk}, .expectation = {.description = "a YES/NO after a warp follows a STEP onto it, which leaves the question up"}},
    {.input = {.name = "no new legs event without a reason the planner can't walk it", .body = test_legs_need_a_reason}, .expectation = {.description = "every walk a planned route, but the listed legs and those whose why says \"not planned: \""}},
};

} // namespace


void add_tests_AutoStorySimulation(UnitTestDatabase& database){
    add_tests_AutoStoryStepTable(database);
    add_tests_AutoStoryStartFrames(database);
    for (const auto& scenario : AUTO_STORY_SIMULATION_CASES){
        database.add<Test_AutoStorySimulation>(scenario);
    }
    for (AutoStoryStageCase& scenario : stage_cases()){
        database.add<Test_AutoStoryStage>(std::move(scenario));
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
