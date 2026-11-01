/*  BDSP AutoStory Story Simulation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run of the whole story table, new game to Floaroma, without the
 *  console: segment_00_table() and segment_01_table() built into segments
 *  as the program builds them (story_table_segment()), run by the shared
 *  runner with the program's retry policy, against a fake game. The fake
 *  plans each ROUTE on the built-in JSON maps from where the run's pose
 *  says we stand, walks each LEGS tile by tile on them, and passes every
 *  other step unless told to fail it. Steps that still call an old
 *  function pass as a whole.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Pokemon_MapSet.h"
#include "Pokemon/Navigation/Pokemon_StorySegments.h"
#include "Pokemon/Navigation/Pokemon_StorySteps.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "StorySegments/PokemonBDSP_AutoStory_Segment_00.h"
#include "StorySegments/PokemonBDSP_AutoStory_Segment_01.h"
#include "Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "Utils/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "Utils/PokemonBDSP_AutoStory_StoryTables.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StorySimulation_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;

std::string describe(const std::vector<TilePos>& tiles){
    std::string text;
    for (const TilePos& pos : tiles){
        const std::optional<Pokemon::MapTile> local = story_maps().to_local(pos);
        text += " " + (local ? local->map + " " + Pokemon::to_string(local->pos) : Pokemon::to_string(pos));
    }
    return text;
}

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n    " + line;
    }
    return text;
}

bool contains(const std::vector<TilePos>& tiles, const TilePos& pos){
    return std::find(tiles.begin(), tiles.end(), pos) != tiles.end();
}

//  Where a ROUTE step ends from `from`, planned as ConsoleStoryExecutor
//  plans it, or why it does not plan.
struct Walked{
    std::vector<TilePos> at;
    std::string error;
    std::string plan;
};
Walked plan_route(const std::vector<TilePos>& from, const Pokemon::StoryStep& step){
    const std::optional<Pokemon::RobustPlanCosts> costs = story_route_costs(step.variant);
    if (!costs){
        return {{}, step.name + ": no plan costs named \"" + step.variant + "\".", ""};
    }
    const std::vector<TilePos> goal = Pokemon::tiles_of(*step.target);
    const auto plan = Pokemon::plan_robust_route(story_maps().world(), from,
        [&](const TilePos& pos){ return contains(goal, pos); }, *costs);
    if (!plan){
        return {{}, step.name + ": no plan from" + describe(from) + ".", ""};
    }
    Walked walked{from, "", std::format("{}: {} legs from{}", step.name, plan->size(), describe(from))};
    for (const Pokemon::PlannedLeg& leg : *plan){
        walked.plan += "\n    " + to_string(leg.leg) + " to" + describe(leg.after);
    }
    if (!plan->empty()){
        walked.at = plan->back().after;
    }
    for (const TilePos& pos : walked.at){
        if (!contains(goal, pos)){
            return {{}, step.name + ": the plan may end off the target, at" + describe({pos}) + ".", walked.plan};
        }
    }
    return walked;
}

//  Where a LEGS step ends from `from`, walked tile by tile on the maps, a
//  tile short too where the step allows it.
Walked walk_legs(const std::vector<TilePos>& from, const Pokemon::StoryStep& step){
    const Pokemon::TileGrid& world = story_maps().world();
    std::vector<TilePos> at = from;
    for (const Pokemon::StoryLeg& leg : step.legs){
        std::vector<TilePos> next;
        for (TilePos pos : at){
            for (int i = 0; i < leg.tiles; i++){
                const std::optional<TilePos> stepped = world.step(pos, leg.direction);
                if (!stepped){
                    return {{}, step.name + ": " + direction_name(leg.direction) + " is blocked at" + describe({pos}) + ".", ""};
                }
                if (step.variant == "one_short_ok" && i + 1 == leg.tiles){
                    next.push_back(pos);
                }
                pos = *stepped;
            }
            next.push_back(pos);
        }
        at = std::move(next);
    }
    return {at, "", ""};
}


//  Saves at a row's start into the trace, as the console runner saves.
class FakeRunner : public StoryStepRunner{
public:
    std::vector<std::string>& trace;
    std::set<std::string> saved;

    explicit FakeRunner(std::vector<std::string>& trace)
        : trace(trace)
    {}
    virtual bool saved_at_start(const std::string& id) const override{
        return saved.contains(id);
    }
    virtual Outcome save_at_start(const std::string& id) override{
        saved.insert(id);
        trace.push_back("save " + id);
        return Pokemon::outcome_ok();
    }
    virtual Outcome run_step(const std::string&, StoryFailure, const StoryAction&) override{
        return Pokemon::outcome_failed("no_console");
    }
};

//  The game: steps pass, routes and legs move the pose on the maps, and
//  the steps in `failures` ("<row>/<step>") fail, so many times, as said.
class FakeGame : public Pokemon::StoryStepExecutor{
public:
    struct Failure{
        std::string failure;
        size_t times = 0;
    };

    std::vector<std::string>& trace;
    std::map<std::string, Failure> failures;
    //  Every route's plan, for the log.
    std::vector<std::string> plans;

    explicit FakeGame(std::vector<std::string>& trace)
        : trace(trace)
    {}

    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
        m_row = data.id;
        trace.push_back("begin " + data.id);
    }
    virtual Outcome run(RunState& state, const Pokemon::StoryStep& step) override{
        const std::string key = m_row + "/" + step.name;
        trace.push_back(key);
        auto failure = failures.find(key);
        if (failure != failures.end() && failure->second.times > 0){
            failure->second.times--;
            return Pokemon::outcome_failed(failure->second.failure, key);
        }
        return Pokemon::StoryStepExecutor::run(state, step);
    }
    virtual Outcome route(RunState& state, const Pokemon::StoryStep& step) override{
        const Walked walked = plan_route(Pokemon::tiles_of(state.pose), step);
        if (!walked.error.empty()){
            return Pokemon::outcome_failed("no_plan", walked.error);
        }
        plans.push_back(walked.plan);
        return Pokemon::outcome_ok(Pokemon::tile_pose(step.target->map, walked.at));
    }
    virtual Outcome legs(RunState& state, const Pokemon::StoryStep& step) override{
        const Walked walked = walk_legs(Pokemon::tiles_of(state.pose), step);
        if (!walked.error.empty()){
            return Pokemon::outcome_failed("blocked", walked.error);
        }
        return Pokemon::outcome_ok(Pokemon::tile_pose(state.pose.map, walked.at));
    }
    virtual Outcome step(RunState&, const Pokemon::StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome mash(RunState&, const Pokemon::StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome battle(RunState&, const Pokemon::StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome train(RunState&, const Pokemon::StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome heal(RunState&, const Pokemon::StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome shop(RunState&, const Pokemon::StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome save(RunState&, const Pokemon::StoryStep&) override{
        trace.push_back("save in " + m_row);
        return Pokemon::outcome_ok();
    }
    virtual Outcome game(RunState&, const Pokemon::StoryStep&) override{ return Pokemon::outcome_ok(); }

private:
    std::string m_row;
};


//  One simulated run of both segments, as the program runs them.
struct Simulation{
    std::vector<std::string> trace;
    std::vector<std::string> lines;
    FakeRunner runner{trace};
    std::shared_ptr<FakeGame> game = std::make_shared<FakeGame>(trace);
    Pokemon::SegmentPolicy policy;

    Pokemon::SegmentReport run(const std::string& id, const std::vector<StoryRow>& table, const Pokemon::SegmentRange& range){
        Pokemon::Segment segment = story_table_segment(runner, id, table, game);
        Pokemon::SegmentHooks hooks;
        hooks.reload = [this](RunState&){
            trace.push_back("reload");
            return Pokemon::outcome_ok();
        };
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        RunState state;
        return Pokemon::run_segment(state, segment, range, policy, hooks);
    }
    //  The rows begun, in order, one per attempt.
    std::vector<std::string> begun() const{
        std::vector<std::string> ids;
        for (const std::string& item : trace){
            if (item.starts_with("begin ")){
                ids.push_back(item.substr(6));
            }
        }
        return ids;
    }
    size_t count(const std::string& item) const{
        return (size_t)std::count(trace.begin(), trace.end(), item);
    }
    bool logged(const std::string& line) const{
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    }
};

std::vector<std::string> ids_of(const std::vector<StoryRow>& table){
    std::vector<std::string> ids;
    for (const StoryRow& row : table){
        ids.push_back(row.id);
    }
    return ids;
}


UnitTestResult test_whole_table(Logger& logger){
    Simulation sim;
    const Pokemon::SegmentReport first = sim.run("00", segment_00_table(), {});
    const Pokemon::SegmentReport second = sim.run("01", segment_01_table(), {});
    for (const std::string& plan : sim.game->plans){
        logger.log(plan);
    }
    if (!first.completed || !second.completed){
        return "Expected both segments through:" + joined(sim.lines);
    }
    std::vector<std::string> expected = ids_of(segment_00_table());
    for (const std::string& id : ids_of(segment_01_table())){
        expected.push_back(id);
    }
    if (sim.begun() != expected){
        return "Unexpected checkpoint order:" + joined(sim.begun());
    }
    //  Every row saves first but 00.00, which a failure ends; 01.10 and
    //  01.11 save again at their ends.
    for (const std::string& id : expected){
        const size_t saves = sim.count("save " + id);
        if (saves != (id == "00.00" ? 0 : 1)){
            return std::format("{} saved {} times at its start:{}", id, saves, joined(sim.trace));
        }
    }
    if (sim.count("save in 01.10") != 1 || sim.count("save in 01.11") != 1){
        return "Expected 01.10 and 01.11 to save at their ends:" + joined(sim.trace);
    }
    if (first.one_shot_passes() != first.checkpoints_run || second.one_shot_passes() != second.checkpoints_run){
        return "Expected every checkpoint one-shot:" + joined(sim.lines);
    }
    return true;
}

UnitTestResult test_start_and_end(Logger&){
    Simulation sim;
    Pokemon::SegmentReport report = sim.run("00", segment_00_table(), {.from = "00.04b", .to = "00.04d"});
    if (!report.completed || sim.begun() != std::vector<std::string>{"00.04b", "00.04c", "00.04d"}){
        return "Expected 00.04b to 00.04d only:" + joined(sim.begun());
    }
    Simulation tail;
    report = tail.run("01", segment_01_table(), {.from = "01.10", .to = std::nullopt});
    if (!report.completed || tail.begun() != std::vector<std::string>{"01.10", "01.11"}){
        return "Expected 01.10 to the end:" + joined(tail.begun());
    }
    Simulation one;
    report = one.run("00", segment_00_table(), {.from = "00.02", .to = "00.02"});
    if (!report.completed || one.begun() != std::vector<std::string>{"00.02"}){
        return "Expected 00.02 alone:" + joined(one.begun());
    }
    return true;
}

//  r72: a blackout in 00.04b's training. The checkpoint resets to the save
//  it made at its start, does not save again, and walks from that save's
//  pose to the grass again.
UnitTestResult test_training_blackout(Logger&){
    Simulation sim;
    sim.game->failures["00.04b/train"] = {.failure = "blackout", .times = 1};
    const Pokemon::SegmentReport report = sim.run("00", segment_00_table(), {.from = "00.04b", .to = "00.04d"});
    const std::vector<std::string> expected{"00.04b", "00.04b", "00.04c", "00.04d"};
    if (!report.completed || report.reloads != 1 || sim.begun() != expected){
        return "Expected 00.04b reloaded once, then on:" + joined(sim.trace);
    }
    if (sim.count("save 00.04b") != 1 || sim.count("00.04b/to_catch_grass") != 2){
        return "Expected the retry to start from 00.04b's one save and walk to the grass again:" + joined(sim.trace);
    }
    if (!sim.logged("[Checkpoint] 00.04b: passed on attempt 2 (1 reset)")){
        return "Expected the retried pass logged:" + joined(sim.lines);
    }
    return true;
}

//  A step that fails every time: three attempts from the checkpoint's
//  save, then the segment stops on it and runs nothing after.
UnitTestResult test_gives_up_after_attempts(Logger&){
    Simulation sim;
    sim.game->failures["01.04/oreburgh_gym"] = {.failure = "operation_failed", .times = 10};
    const Pokemon::SegmentReport report = sim.run("01", segment_01_table(), {});
    if (report.completed || report.reloads != 2 || report.failed_checkpoint != "01.04" || report.failed_event != "oreburgh_gym"
        || report.runs.empty() || report.runs.back().attempts != 3 || sim.count("01.04/oreburgh_gym") != 3
        || sim.count("save 01.04") != 1 || sim.count("begin 01.05") != 0
    ){
        return "Expected 01.04 tried 3 times from its save, then a stop:" + joined(sim.trace) + joined(sim.lines);
    }
    //  The stop-at-first-failure option: no reload.
    Simulation lab;
    lab.policy.stop_at_first_failure = true;
    lab.game->failures["01.04/oreburgh_gym"] = {.failure = "operation_failed", .times = 10};
    const Pokemon::SegmentReport stopped = lab.run("01", segment_01_table(), {});
    if (stopped.completed || !stopped.stopped_at_first_failure || lab.count("reload") != 0){
        return "Expected a stop at the first failure without a reload:" + joined(lab.trace);
    }
    return true;
}

//  Each row that has a start pose starts where the row before it leaves
//  the player, by the same planning and walking the fake run does.
UnitTestResult test_poses_chain(Logger&){
    for (const std::vector<StoryRow>* table : {&segment_00_table(), &segment_01_table()}){
        std::optional<std::vector<TilePos>> at;
        std::string last = "";
        for (const StoryRow& row : *table){
            if (row.start){
                const std::vector<TilePos> start = Pokemon::tiles_of(*row.start);
                if (at && !std::ranges::all_of(*at, [&](const TilePos& pos){ return contains(start, pos); })){
                    return std::format("{} starts at{}, but {} may end at{}.", row.id, describe(start), last, describe(*at));
                }
                at = start;
            }else{
                at.reset();
            }
            for (const Pokemon::StoryStep& step : row.steps){
                if (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS){
                    if (!at){
                        return row.id + " " + step.name + ": walks from an unknown pose.";
                    }
                    const Walked walked = step.kind == StoryStepKind::ROUTE ? plan_route(*at, step) : walk_legs(*at, step);
                    if (!walked.error.empty()){
                        return row.id + " " + walked.error;
                    }
                    at = walked.at;
                }else if (step.kind == StoryStepKind::GAME || step.kind == StoryStepKind::SHOP){
                    //  A call to an old function: where it ends is the table's to say.
                    at.reset();
                }
                if (step.pose_after){
                    at = Pokemon::tiles_of(*step.pose_after);
                }
            }
            last = row.id;
        }
    }
    return true;
}

//  Every step that calls an old function names one the segment registers,
//  and each table lints clean.
UnitTestResult test_variants_registered(Logger&){
    const struct{ const std::vector<StoryRow>* table; std::map<std::string, ConsoleStoryExecutor::Variant> variants; } segments[] = {
        {.table = &segment_00_table(), .variants = segment_00_variants()},
        {.table = &segment_01_table(), .variants = segment_01_variants()},
    };
    const std::set<std::string> built_in{"", "town", "robust", "one_short_ok"};
    for (const auto& segment : segments){
        for (const StoryRow& row : *segment.table){
            const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(row);
            if (!problems.empty()){
                return problems[0];
            }
            for (const Pokemon::StoryStep& step : row.steps){
                if (!built_in.contains(step.variant) && !segment.variants.contains(step.variant)){
                    return std::format("{} {}: no variant \"{}\" is registered.", row.id, step.name, step.variant);
                }
            }
        }
    }
    return true;
}

//  r261: out of the Center the walk saw no dark fade and walked on three
//  rows. The walk stops at the first look that is not a room, fade or not.
UnitTestResult test_door_exit_looks(Logger&){
    const struct{ const char* name; std::vector<std::pair<double, bool>> looks; size_t stops_at; } cases[] = {
        {.name = "no fade: the first look outside", .looks = {{180, true}, {175, true}, {230, false}}, .stops_at = 2},
        {.name = "the fade, then outside", .looks = {{180, true}, {40, true}, {210, false}}, .stops_at = 2},
        {.name = "a dim look outside", .looks = {{180, true}, {150, false}, {230, false}}, .stops_at = 1},
        {.name = "still a room", .looks = {{180, true}, {40, true}, {190, true}}, .stops_at = 3},
    };
    for (const auto& c : cases){
        DoorFadeWatch watch;
        size_t stopped = c.looks.size();
        for (size_t i = 0; i < c.looks.size(); i++){
            if (door_walk_stops(watch, c.looks[i].first, c.looks[i].second)){
                stopped = i;
                break;
            }
        }
        if (stopped != c.stops_at){
            return std::format("{}: stopped at look {}, expected {}.", c.name, stopped, c.stops_at);
        }
    }
    return true;
}


class Test_StorySimulation : public UnitTest{
public:
    Test_StorySimulation(std::string name, std::function<UnitTestResult(Logger&)> body)
        : UnitTest("PokemonBDSP::AutoStory - Simulation: " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult(Logger&)> m_body;
};

}


void add_tests_AutoStory_StorySimulation(UnitTestDatabase& database){
    database.add<Test_StorySimulation>("00.00 to Floaroma, in order, each row saved first", test_whole_table);
    database.add<Test_StorySimulation>("start and end checkpoints", test_start_and_end);
    database.add<Test_StorySimulation>("a blackout in 00.04b's training resets to its save", test_training_blackout);
    database.add<Test_StorySimulation>("a step that keeps failing stops after 3 attempts", test_gives_up_after_attempts);
    database.add<Test_StorySimulation>("each start pose is where the row before ends; routes plan on the maps", test_poses_chain);
    database.add<Test_StorySimulation>("every called function is registered; tables lint", test_variants_registered);
    database.add<Test_StorySimulation>("out of a door: the first look that is not a room", test_door_exit_looks);
}



}
}
}
