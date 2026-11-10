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
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryPaceSim.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTable.h"

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
    virtual Outcome run(RunState& state, const StoryStep& step) override{
        const std::string key = m_row + "/" + step.name;
        trace.push_back(key);
        auto failure = failures.find(key);
        if (failure != failures.end() && failure->second.times > 0){
            failure->second.times--;
            return Pokemon::outcome_failed(failure->second.failure, key);
        }
        return StoryMapsSim::run(state, step);
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
                    add_start_pose_check(checkpoint, start_pose(data), [this]{ return game.save_stands; }, [this](const std::string& line){ lines.push_back(line); });
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
        return Pokemon::run_segment(state, segment, range, policy, hooks);
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
};

struct AutoStorySimulationExpectation{
    std::string description;
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
        try{
            return m_scenario.input.logged ? m_scenario.input.logged(logger) : m_scenario.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStorySimulationTestCase m_scenario;
};


UnitTestResult test_whole_story(){
    Simulation sim;
    const Pokemon::SegmentReport report = sim.run({});
    if (!report.completed){
        return "Expected the whole story through:" + joined(sim.lines);
    }
    std::vector<std::string> ids;
    for (const StoryCheckpoint& data : sim.rows){
        ids.push_back(data.id);
    }
    if (sim.begun() != ids || sim.saves.size() != ids.size()){
        return "Expected each checkpoint once, in order, each saving at its end:" + joined(sim.game.trace);
    }
    //  Each checkpoint starts where the save before it stands.
    for (size_t c = 1; c < sim.rows.size(); c++){
        const StoryCheckpoint& data = sim.rows[c];
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
    //  00.07's shopping, with the cursor staying on each row bought.
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

//  The whole story through the table as the simulation binds it, each
//  row's stubs logged: a step is stubbed exactly when its action needs the
//  console, so no step the simulation plays is passed over.
UnitTestResult test_stubbed_steps_need_the_console(Logger& logger){
    Simulation sim;
    if (!sim.run({}).completed){
        return "Expected the whole story through:" + joined(sim.lines);
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
    const Pokemon::SegmentReport report = sim.run({.from = "00.05", .to = "00.07"});
    if (!report.completed || sim.begun() != std::vector<std::string>{"00.05", "00.06", "00.07"}){
        return "Expected 00.05 to 00.07 only:" + joined(sim.begun());
    }
    return true;
}

//  A walk that fails once (a trainer the walker could not recover from):
//  the checkpoint reloads its last save and runs again from its start.
UnitTestResult test_retry_from_save(){
    Simulation sim;
    sim.game.failures["00.08/forest_north"] = {.failure = "operation_failed", .times = 1};
    const Pokemon::SegmentReport report = sim.run({.from = "00.08", .to = "00.09"});
    if (!report.completed || report.reloads != 1 || sim.begun() != std::vector<std::string>{"00.08", "00.08", "00.09"}
        || sim.count("00.08/forest_east") != 2
    ){
        return "Expected 00.08 reloaded once and run again from its start:" + joined(sim.game.trace);
    }
    if (std::find(sim.lines.begin(), sim.lines.end(), "[Checkpoint] 00.08: passed on attempt 2 (1 reset)") == sim.lines.end()){
        return "Expected the retried pass logged:" + joined(sim.lines);
    }
    return true;
}

//  console B, FRLG 00.27, 2026-11-09: the save stood below Cerulean Center's
//  door (22, 19), and the run walked as if from the counter where 00.27
//  starts. It, and 00.09 from below Pewter Center's door (17, 25), stop
//  before any step or save.
UnitTestResult test_save_off_start_pose(){
    const std::pair<std::string, StoryPose> CASES[] = {
        {"00.09", StoryPose{"PewterCity", TilePos{17, 26}, Direction::SOUTH}},
        {"00.27", StoryPose{"CeruleanCity", TilePos{22, 20}, Direction::SOUTH}},
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

//  Brock's battle lost every time: three attempts, then the stop.
UnitTestResult test_gives_up_after_attempts(){
    Simulation sim;
    sim.game.failures["00.09/brock"] = {.failure = "blackout", .times = 10};
    const Pokemon::SegmentReport report = sim.run({.from = "00.09", .to = std::nullopt});
    if (report.completed || report.reloads != 2 || report.failed_checkpoint != "00.09" || report.failed_event != "brock"
        || report.failure.failure != "blackout" || sim.count("00.09/brock") != 3 || !sim.saves.empty()
    ){
        return "Expected 00.09 tried 3 times, then a stop on Brock:" + joined(sim.game.trace) + joined(sim.lines);
    }
    return true;
}

//  Walks `step` of 00.14 on `sim` from `at`, where the table says the
//  player stands, and moves `at` to where it says the step ends. Returns
//  the failure, or empty.
std::string walk_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step){
    const std::string where = "00.14/" + step.name + ": ";
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
        const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
        if (sim.state().facing != direction){
            sim.tap(direction);
        }
        sim.tap(direction);
        sim.watch({SimWatcher::MESSAGE}, std::chrono::milliseconds(2000));
        at = *story_pose(Pokemon::pose_of(*step.pose_after));
    }
    if (!open_end && !(sim.state().position == MapPosition{at.map, at.tile})){
        return where + "expected " + to_string(MapPosition{at.map, at.tile}) + ", but on " + to_string(sim.state().position) + ".";
    }
    return "";
}

//  Console B, 2026-11-03 (row 00.14): the open leg east from Route 4
//  into Cerulean City ended a tile short, on (6, 20), with nothing on
//  screen. No hold to a wall finds the player there, and the walk went on
//  a column off, the hold south stopped by the fence on (6, 23), to a
//  wall below a row of houses, not the Pokemon Center's door. From the
//  foot of Route 4's ledge, the row's walks on the sim reach the nurse
//  with that leg whole or a tile short.
UnitTestResult test_cerulean_entry_short(){
    const StoryCheckpoint* row = &checkpoint_row("00.14");
    const auto hop = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "ledge_hop"; });
    if (hop == row->steps.end() || !hop->pose_after){
        return "00.14: no ledge_hop.";
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

//  Console B, 2026-11-03 (row 00.16), twice: the open leg north from
//  Cerulean City onto Nugget Bridge ended on (11, 33), a tile short of
//  (11, 32), and the one step onto Cale's row reached only (11, 32), out
//  of his sight: "bug_catcher_cale: failed: no_battle". From the walk's
//  end, whole or a tile short, the five trainers are met and beaten on
//  the sim, each battle on the trainer's row.
UnitTestResult test_nugget_bridge_short(){
    const StoryCheckpoint* row = &checkpoint_row("00.16");
    const auto walk = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "below_cale_line"; });
    if (walk == row->steps.end() || !walk->args.contains("target")){
        return "No step 00.16/below_cale_line.";
    }
    for (int short_by : {0, 1}){
        StoryPose at = *story_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(walk->args, "target")));
        OverworldSim sim(story_maps(), MapPosition{at.map, {at.tile.x, at.tile.y + short_by}}, Direction::NORTH);
        for (auto step = walk + 1; step != row->steps.end() && step->kind != StoryStepKind::READY; ++step){
            const std::string failure = row_step_on_sim(sim, at, *step, "00.16");
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

//  00.17 on the sim, from its start on Route 24 to the Sea Cottage:
//  Franklin, Kelsey, Nob and Haley are met on the steps into their lines
//  and beaten there. The sim's trainers who look around see every way
//  they may look, so Youngster Dan sees the player on (20, 4), walks up
//  to (21, 4) and is fought there, off the way on.
UnitTestResult test_route25_trainers(){
    const StoryCheckpoint* row = &checkpoint_row("00.17");
    StoryPose at = *story_pose(Pokemon::pose_of(*row->start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row->steps){
        const std::string failure = row_step_on_sim(sim, at, step, "00.17");
        if (!failure.empty()){
            return failure;
        }
    }
    const std::vector<std::string> expected{"hiker", "youngster", "picnicker", "hiker", "lass"};
    if (sim.state().trainers_beaten != expected){
        std::string beaten;
        for (const std::string& name : sim.state().trainers_beaten){
            beaten += " " + name;
        }
        return "00.17: expected Franklin, Dan, Kelsey, Nob and Haley beaten; beat" + beaten + ".";
    }
    if (!(sim.state().position == MapPosition{"Route25_SeaCottage", {7, 8}})){
        return "00.17: expected to end in the Sea Cottage on (7, 8), not " + to_string(sim.state().position) + ".";
    }
    return true;
}

//  00.20 on the sim, its walks from the Center: over Nugget Bridge, whose
//  trainers 00.16 beat (they stand there, but don't battle), onto Route
//  24's grass at (4, 19) out of Camper Shane's sight, and back to the
//  nurse. No trainer battles on the way. The open leg north from Cerulean
//  City onto the bridge crosses onto Route 24, where the game holds the
//  player up a tile: held for its tiles alone it ended on (11, 16), a tile
//  short, and the leg west bumped the bridge's posts (console B,
//  2026-11-03, row 00.20, twice).
//  The story maps after 00.16: Nugget Bridge's trainers beaten, standing
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
    const StoryCheckpoint* row = &checkpoint_row("00.20");
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
        const std::string failure = row_step_on_sim(sim, at, step, "00.20");
        if (!failure.empty()){
            return failure;
        }
        const auto end = ends.find(step.name);
        if (end == ends.end() || !(sim.state().position == end->second)){
            return "00.20/" + step.name + ": ended on " + to_string(sim.state().position) + ".";
        }
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "00.20: a trainer's battle or a violation on the way.";
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

//  Console B, 2026-11-03 (row 00.20, run m61): the Oddish's battle came
//  on the pace's far step, so the capture left the player on (3, 19), not
//  the table's (4, 19). The walk back planned from (4, 19) went a column
//  west of its plan, down the bridge's column 10, not 11, and stopped
//  against the beaten Lass on (10, 22); every later leg bumped, and the
//  warp to the Center never came. On the sim, the battle on either step
//  of the pace, the row's later walks, from the capture's tiles, reach the
//  nurse.
//  Console B, 2026-11-03 (m63, 00.20 verify run 2): the open leg west
//  from Nugget Bridge's head ended a tile short, so the walk down onto the
//  grass ended on (5, 19), in Camper Shane's sight; after the first wild
//  battle he walked up ("I saw your feat from the grass!") and the capture
//  stopped. From the Mart, the walk to the patch on the sim keeps out of
//  his sight with the leg west whole or a tile short.
UnitTestResult test_oddish_patch_west_short(){
    const StoryCheckpoint* row = &checkpoint_row("00.20");
    const auto mart = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "mart_clerk"; });
    const auto patch = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "oddish_patch"; });
    if (mart == row->steps.end() || patch == row->steps.end() || !target_of(*mart) || !target_of(*patch)){
        return "00.20: no mart_clerk or oddish_patch.";
    }
    for (int short_by : {0, 1}){
        StoryPose at = *story_pose(*target_of(*mart));
        SimScript script;
        //  Holds from the Mart: south, west, north, east, north, east, north
        //  over the bridge, then the 8th, west from the bridge's head.
        script.short_holds[8] = short_by;
        OverworldSim sim(maps_bridge_beaten(), MapPosition{at.map, at.tile}, *at.facing, script);
        const std::string failure = row_step_on_sim(sim, at, *patch, "00.20");
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
    const StoryCheckpoint* row = &checkpoint_row("00.20");
    const auto capture = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "catch_oddish"; });
    if (capture == row->steps.end() || capture + 1 == row->steps.end() || !target_of(*(capture + 1))){
        return "00.20: no catch_oddish with a route after it.";
    }
    const auto patch = std::find_if(row->steps.begin(), capture, [](const StoryStep& step){ return step.name == "oddish_patch"; });
    if (patch == capture || !target_of(*patch)){
        return "00.20: no oddish_patch before catch_oddish.";
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
        if (const std::string failure = walk_paced_route(sim, *route, "00.20/" + back.name); !failure.empty()){
            return label + failure;
        }
        for (auto step = capture + 2; step != row->steps.end(); ++step){
            const std::string failure = row_step_on_sim(sim, at, *step, "00.20");
            if (!failure.empty()){
                return label + failure;
            }
        }
        if (!(sim.state().position == MapPosition{"CeruleanCity_PokemonCenter_1F", {7, 4}})){
            return label + "00.20 ended on " + to_string(sim.state().position) + ", not the nurse.";
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
        return std::format("Expected the routes after 00.14b's and 00.15's GRINDs and 00.20's capture; checked {}.", checked);
    }
    return true;
}

//  00.14 on the sim, from Mt. Moon's exit (32, 6): east and up through
//  (48, 5) into the tutors' pen on Route 4 to (50, 4), facing the Mega
//  Kick tutor (50, 3), then on to above the ledge (93, 8). Each walk ends
//  where the table says, and no trainer sees the player on the way.
UnitTestResult test_mega_kick_tutor_walks(){
    const StoryCheckpoint* row = &checkpoint_row("00.14");
    const auto ledge = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "above_ledge"; });
    if (ledge == row->steps.end()){
        return "00.14: no above_ledge.";
    }
    StoryPose at = *story_pose(Pokemon::pose_of(*row->start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    bool at_tutor = false;
    for (auto step = row->steps.begin(); step != ledge + 1; ++step){
        if (step->kind != StoryStepKind::ROUTE && step->kind != StoryStepKind::LEGS){
            continue;
        }
        const std::string failure = row_step_on_sim(sim, at, *step, "00.14");
        if (!failure.empty()){
            return failure;
        }
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return "00.14/" + step->name + ": expected " + to_string(MapPosition{at.map, at.tile}) + ", but on " + to_string(sim.state().position) + ".";
        }
        if (sim.state().position == MapPosition{"Route4", {50, 4}} && sim.state().facing == Direction::NORTH){
            at_tutor = true;
        }
    }
    if (!at_tutor){
        return "00.14: no walk ended on Route 4's (50, 4) facing the Mega Kick tutor (50, 3).";
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "00.14: a trainer's battle or a violation on the way.";
    }
    return true;
}

//  Console B, 2026-11-03 (row 00.18): after his scene Bill stands on
//  (7, 6), east of the player on (6, 6), not on the maps' (7, 5). The
//  walk out planned east onto (7, 6) and south to the mat (7, 9); the
//  east tap bumped Bill, and the hold south took the player down column 6
//  to (6, 9), beside the mat, with no warp. On a copy of the maps with
//  Bill on (7, 6) and only the mat (7, 9) as the way out (the console
//  didn't warp from (6, 9)), the row's walks after the ticket end each
//  where the table says and leave on Route 25's (51, 5).
UnitTestResult test_out_of_bill_cottage(){
    const StoryCheckpoint* row = &checkpoint_row("00.18");
    const auto ticket = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "ss_ticket"; });
    if (ticket == row->steps.end() || !ticket->pose_after){
        return "00.18: no ss_ticket.";
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
        const std::string failure = row_step_on_sim(sim, at, *step, "00.18");
        if (!failure.empty()){
            return failure;
        }
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return "00.18/" + step->name + ": expected " + to_string(MapPosition{at.map, at.tile}) + ", but on " + to_string(sim.state().position) + ".";
        }
    }
    if (!(sim.state().position == MapPosition{"Route25", {51, 5}})){
        return "00.18: expected to leave onto Route 25's (51, 5), not " + to_string(sim.state().position) + ".";
    }
    if (!sim.state().violations.empty()){
        return "00.18: violations:" + joined(sim.state().violations);
    }
    return true;
}


//  Someone talks to the player before every step (StorySimKnobs::
//  dialog_before): cleared between steps, the whole story still passes;
//  left up, as before the executor looked, a walk fails on it.
UnitTestResult test_dialog_before_every_step(){
    const auto always = [](const std::string&, const std::string&){
        return true;
    };
    Simulation cleared({.dialog_before = always});
    if (!cleared.run({}).completed){
        return "Expected the story through, a dialog before every step cleared:" + joined(cleared.lines);
    }
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
UnitTestResult test_dialogs_at_random(Logger& logger){
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
            passes[clear] += sim.run({}).completed ? 1 : 0;
        }
    }
    logger.log(std::format(
        "Dialogs before one step in five, {} seeds: {} passed left up, {} cleared between steps.",
        SEEDS, passes[0], passes[1]
    ));
    if (passes[1] != SEEDS){
        return std::format("Expected every run through with the dialogs cleared, {} of {}.", passes[1], SEEDS);
    }
    return true;
}


const AutoStorySimulationTestCase AUTO_STORY_SIMULATION_CASES[] = {
    {.input = {.name = "new game to Brock, in order, each start where the last save stood", .body = test_whole_story}, .expectation = {.description = "runs full story progression from new game through Brock gym"}},
    {.input = {.name = "the steps it stubs need the console", .body = nullptr, .logged = test_stubbed_steps_need_the_console}, .expectation = {.description = "stubs each needs-console step of the whole story and plays the rest"}},
    {.input = {.name = "start and end checkpoints", .body = test_start_and_end}, .expectation = {.description = "executes segment bounded by start and end checkpoints"}},
    {.input = {.name = "a failed walk reloads its checkpoint's save", .body = test_retry_from_save}, .expectation = {.description = "reloads checkpoint save state on simulated movement failure"}},
    {.input = {.name = "a battle lost every time stops after 3 attempts", .body = test_gives_up_after_attempts}, .expectation = {.description = "aborts after three consecutive battle blackouts"}},
    {.input = {.name = "00.14 into Cerulean, the entry leg a tile short", .body = test_cerulean_entry_short}, .expectation = {.description = "recovers when entering Cerulean City if movement ends one tile short"}},
    {.input = {.name = "00.14 from Mt. Moon to the Mega Kick tutor, then above the ledge", .body = test_mega_kick_tutor_walks}, .expectation = {.description = "verifies route navigation from Mt. Moon to tutor and past ledge"}},
    {.input = {.name = "00.16 Nugget Bridge, the walk onto it a tile short", .body = test_nugget_bridge_short}, .expectation = {.description = "completes Nugget Bridge battles even if initial approach stops short"}},
    {.input = {.name = "00.17 Route 25, its four trainers and Dan", .body = test_route25_trainers}, .expectation = {.description = "simulates Route 25 trainer battles and Dan detour"}},
    {.input = {.name = "00.18 out of Bill's Sea Cottage, Bill on (7, 6)", .body = test_out_of_bill_cottage}, .expectation = {.description = "simulates exiting Bill sea cottage with Bill relocated"}},
    {.input = {.name = "00.20 to Route 24's grass and back, past Shane", .body = test_oddish_walks}, .expectation = {.description = "navigates to Route 24 grass avoiding trainer line of sight"}},
    {.input = {.name = "00.20 back to the Center, the Oddish's battle on the pace's far step", .body = test_oddish_battle_on_far_step}, .expectation = {.description = "handles Oddish capture encounter occurring on far pace step"}},
    {.input = {.name = "00.20 onto the grass out of Shane's sight, the leg west a tile short", .body = test_oddish_patch_west_short}, .expectation = {.description = "recovers when approaching grass patch with short western leg"}},
    {.input = {.name = "the routes after pacing, from either of its tiles", .body = test_routes_after_pacing}, .expectation = {.description = "plans valid routes regardless of which pacing tile character ends on"}},
    {.input = {.name = "a save off the start pose stops before any step", .body = test_save_off_start_pose}, .expectation = {.description = "fails the start check without walking or saving"}},
    {.input = {.name = "a dialog pops up before every step", .body = test_dialog_before_every_step}, .expectation = {.description = "cleared between steps, the story passes; left up, a walk fails on it"}},
    {.input = {.name = "dialogs pop up before one step in five, over 5 seeds", .body = nullptr, .logged = test_dialogs_at_random}, .expectation = {.description = "every run passes with them cleared"}},
};

} // namespace


void add_tests_AutoStorySimulation(UnitTestDatabase& database){
    add_tests_AutoStoryStepTable(database);
    add_tests_AutoStoryStartFrames(database);
    for (const auto& scenario : AUTO_STORY_SIMULATION_CASES){
        database.add<Test_AutoStorySimulation>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
