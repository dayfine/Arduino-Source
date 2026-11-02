/*  BDSP AutoStory Story Simulation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run of the whole story table, new game to Floaroma, without the
 *  console: story_checkpoints() built into one segment as the program builds
 *  it (make_story_segment()), run by the shared runner with the program's
 *  retry policy, against a fake game. The fake plans each ROUTE on the story
 *  world that holds its start and target, walks each LEGS tile by tile, and
 *  passes every other step unless told to fail it. Events pass as a whole.
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
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StorySimulation_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;

std::string describe(const std::string& map, const std::vector<TilePos>& tiles){
    std::string text = " " + map;
    for (const TilePos& pos : tiles){
        text += " " + Pokemon::to_string(pos);
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

//  Where a step ends, or why it does not plan.
struct Walked{
    std::optional<Pose> at;
    std::string error;
    std::string plan;
};

//  A ROUTE step from `from`, planned as StoryExecutor::walk_to() walks it.
Walked plan_route(const Pose& from, const StoryStep& step){
    const Pose& goal = *step.target;
    const StoryWorld* world = story_plan_world(from.map, goal.map);
    const std::optional<RobustPlanCosts> costs = story_route_costs(step.variant);
    if (world == nullptr || !costs){
        return {std::nullopt, std::format("{}: no world joins {} and {}, or no costs \"{}\".", step.name, from.map, goal.map, step.variant), ""};
    }
    std::vector<TilePos> start;
    for (const TilePos& pos : Pokemon::tiles_of(from)){
        start.push_back(story_to_world(*world, from.map, pos));
    }
    std::vector<TilePos> targets;
    for (const TilePos& pos : Pokemon::tiles_of(goal)){
        targets.push_back(story_to_world(*world, goal.map, pos));
    }
    const auto plan = plan_robust_route(*world, start, [&](const TilePos& pos){ return contains(targets, pos); }, *costs);
    if (!plan){
        return {std::nullopt, step.name + ": no plan from" + describe(from.map, Pokemon::tiles_of(from)) + ".", ""};
    }
    Walked walked{std::nullopt, "", std::format("{}: {} legs", step.name, plan->size())};
    std::vector<TilePos> local;
    for (const PlannedLeg& leg : *plan){
        walked.plan += " " + to_string(leg.leg);
    }
    for (const TilePos& pos : plan->empty() ? start : plan->back().after){
        const std::optional<TilePos> tile = story_to_local(*world, goal.map, pos);
        if (!tile || !contains(Pokemon::tiles_of(goal), *tile)){
            return {std::nullopt, step.name + ": the plan may end off the target, at " + describe(*world, pos) + ".", walked.plan};
        }
        local.push_back(*tile);
    }
    walked.at = Pokemon::tile_pose(goal.map, local, Pokemon::facing_of(goal));
    return walked;
}

//  `leg` of `step` from `pos`: adds where it ends to `next` (and a tile
//  short where the step allows it). Returns why it is blocked, or "".
std::string walk_leg_from(
    const StoryWorld& world, TilePos pos, const Pokemon::StoryLeg& leg, const StoryStep& step,
    std::vector<TilePos>& next
){
    for (int i = 0; i < leg.tiles; i++){
        const std::optional<TilePos> stepped = PokemonBDSP::step(world, pos, leg.direction);
        if (!stepped){
            return step.name + ": " + direction_name(leg.direction) + " is blocked at " + describe(world, pos) + ".";
        }
        if (step.variant == "one_short_ok" && i + 1 == leg.tiles){
            next.push_back(pos);
        }
        pos = *stepped;
    }
    next.push_back(pos);
    return "";
}

//  A LEGS step from `from`, walked tile by tile on its map, a tile short too
//  where the step allows it. Walks to walls end where nothing is known.
Walked walk_legs(const Pose& from, const StoryStep& step){
    if (step.variant == "to_wall"){
        return {std::nullopt, "", ""};
    }
    const StoryWorld* world = story_plan_world(from.map, from.map);
    if (world == nullptr){
        return {std::nullopt, step.name + ": no world holds " + from.map + ".", ""};
    }
    std::vector<TilePos> at;
    for (const TilePos& pos : Pokemon::tiles_of(from)){
        at.push_back(story_to_world(*world, from.map, pos));
    }
    for (const Pokemon::StoryLeg& leg : step.legs){
        std::vector<TilePos> next;
        for (const TilePos& pos : at){
            const std::string blocked = walk_leg_from(*world, pos, leg, step, next);
            if (!blocked.empty()){
                return {std::nullopt, blocked, ""};
            }
        }
        at = std::move(next);
    }
    std::vector<TilePos> local;
    for (const TilePos& pos : at){
        local.push_back(*story_to_local(*world, from.map, pos));
    }
    return {Pokemon::tile_pose(from.map, local, step.legs.back().direction), "", ""};
}

//  A STEP onto the next tile of the same map.
Walked step_on(const Pose& from, const StoryStep& step){
    std::vector<TilePos> at;
    for (const TilePos& pos : Pokemon::tiles_of(from)){
        at.push_back(pos.next(step.direction));
    }
    return {Pokemon::tile_pose(from.map, at, step.direction), "", ""};
}


//  The game: steps pass, routes, legs and steps move the pose on the maps,
//  and the steps in `failures` ("<row>/<step>") fail, so many times.
class FakeGame : public Pokemon::StoryStepExecutor{
public:
    struct Failure{
        std::string failure;
        size_t times = 0;
    };

    std::vector<std::string>& trace;
    std::map<std::string, Failure> failures;
    std::vector<std::string> plans;

    explicit FakeGame(std::vector<std::string>& trace)
        : trace(trace)
    {}

    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
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
        return Pokemon::StoryStepExecutor::run(state, step);
    }
protected:
    virtual Outcome route(RunState& state, const StoryStep& step) override{
        return moved(plan_route(state.pose, step), "no_plan");
    }
    virtual Outcome legs(RunState& state, const StoryStep& step) override{
        return moved(walk_legs(state.pose, step), "blocked");
    }
    virtual Outcome step(RunState& state, const StoryStep& step) override{
        return moved(step_on(state.pose, step), "blocked");
    }
    virtual Outcome new_game(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome options(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome talk(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome mash(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome battle(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome train(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome heal(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome shop(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome save(RunState&, const StoryStep&) override{
        trace.push_back("save in " + m_row);
        return Pokemon::outcome_ok();
    }
    virtual Outcome game(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }

private:
    Outcome moved(const Walked& walked, const char* failure){
        if (!walked.error.empty()){
            return Pokemon::outcome_failed(failure, walked.error);
        }
        if (!walked.plan.empty()){
            plans.push_back(walked.plan);
        }
        return walked.at ? Pokemon::outcome_ok(*walked.at) : Pokemon::outcome_ok(Pose{});
    }

    std::string m_row;
};


//  One simulated run, as the program runs it, in `language`. Japanese has
//  no text reader, so a step that needed text would show. (The console's
//  global language is left alone: tests run in parallel.)
struct Simulation{
    std::vector<std::string> trace;
    std::vector<std::string> lines;
    FakeGame game{trace};
    std::set<std::string> saved;
    Pokemon::SegmentPolicy policy;

    Pokemon::SegmentReport run(const Pokemon::SegmentRange& range, Language language = Language::Japanese){
        game.set_language(language);
        Pokemon::Segment segment = make_story_segment(story_checkpoints(), game, saved);
        Pokemon::SegmentHooks hooks;
        hooks.reload = [this](RunState&){
            trace.push_back("reload");
            return Pokemon::outcome_ok();
        };
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        RunState state;
        return Pokemon::run_segment(state, segment, range, policy, hooks);
    }
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

std::vector<std::string> all_ids(){
    std::vector<std::string> ids;
    for (const StoryCheckpoint& row : story_checkpoints()){
        ids.push_back(row.id);
    }
    return ids;
}


UnitTestResult test_whole_table(Logger& logger){
    for (Language language : {Language::English, Language::Japanese}){
        Simulation sim;
        const Pokemon::SegmentReport report = sim.run({}, language);
        for (const std::string& plan : sim.game.plans){
            logger.log(plan);
        }
        if (!report.completed){
            return std::string(language_data(language).name) + ": expected the whole story through:" + joined(sim.lines);
        }
        if (sim.begun() != all_ids()){
            return "Unexpected checkpoint order:" + joined(sim.begun());
        }
        //  Every row saves first but 00.00, which a failure ends; 01.10 and
        //  01.11 save again at their ends.
        for (const std::string& id : all_ids()){
            const size_t saves = sim.count("save in " + id);
            const size_t expected = id == "00.00" ? 0 : id == "01.10" || id == "01.11" ? 2 : 1;
            if (saves != expected){
                return std::format("{} saved {} times, expected {}:{}", id, saves, expected, joined(sim.trace));
            }
        }
        if (report.one_shot_passes() != report.checkpoints_run){
            return "Expected every checkpoint one-shot:" + joined(sim.lines);
        }
    }
    return true;
}

UnitTestResult test_start_and_end(Logger&){
    Simulation sim;
    Pokemon::SegmentReport report = sim.run({.from = "00.04b", .to = "00.04d"});
    if (!report.completed || sim.begun() != std::vector<std::string>{"00.04b", "00.04c", "00.04d"}){
        return "Expected 00.04b to 00.04d only:" + joined(sim.begun());
    }
    Simulation tail;
    report = tail.run({.from = "01.10", .to = std::nullopt});
    if (!report.completed || tail.begun() != std::vector<std::string>{"01.10", "01.11"}){
        return "Expected 01.10 to the end:" + joined(tail.begun());
    }
    Simulation one;
    report = one.run({.from = "00.02", .to = "00.02"});
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
    sim.game.failures["00.04b/train"] = {.failure = "blackout", .times = 1};
    const Pokemon::SegmentReport report = sim.run({.from = "00.04b", .to = "00.04d"});
    const std::vector<std::string> expected{"00.04b", "00.04b", "00.04c", "00.04d"};
    if (!report.completed || report.reloads != 1 || sim.begun() != expected){
        return "Expected 00.04b reloaded once, then on:" + joined(sim.trace);
    }
    if (sim.count("save in 00.04b") != 1 || sim.count("00.04b/to_catch_grass") != 2){
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
    const std::string step = "01.04/gym_until_roark_beaten";
    Simulation sim;
    sim.game.failures[step] = {.failure = "operation_failed", .times = 10};
    const Pokemon::SegmentReport report = sim.run({.from = "01.01", .to = std::nullopt});
    if (report.completed || report.reloads != 2 || report.failed_checkpoint != "01.04" || report.failed_event != "gym_until_roark_beaten"
        || report.runs.empty() || report.runs.back().attempts != 3 || sim.count(step) != 3
        || sim.count("save in 01.04") != 1 || sim.count("begin 01.05") != 0
    ){
        return "Expected 01.04 tried 3 times from its save, then a stop:" + joined(sim.trace) + joined(sim.lines);
    }
    Simulation lab;
    lab.policy.stop_at_first_failure = true;
    lab.game.failures[step] = {.failure = "operation_failed", .times = 10};
    const Pokemon::SegmentReport stopped = lab.run({.from = "01.01", .to = std::nullopt});
    if (stopped.completed || !stopped.stopped_at_first_failure || lab.count("reload") != 0){
        return "Expected a stop at the first failure without a reload:" + joined(lab.trace);
    }
    return true;
}

//  Each row that has a start pose starts where the row before it leaves
//  the player, by the same planning and walking the fake run does.
UnitTestResult test_poses_chain(Logger&){
    std::optional<Pose> at;
    std::string last;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.start){
            const std::vector<TilePos> start = Pokemon::tiles_of(*row.start);
            if (at && (at->map != row.start->map ||
                !std::ranges::all_of(Pokemon::tiles_of(*at), [&](const TilePos& pos){ return contains(start, pos); }))
            ){
                return std::format("{} starts at{}, but {} may end at{}.", row.id, describe(row.start->map, start),
                    last, describe(at->map, Pokemon::tiles_of(*at)));
            }
            at = row.start;
        }else{
            at.reset();
        }
        for (const StoryStep& step : row.steps){
            if (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS || step.kind == StoryStepKind::STEP){
                if (!at && step.variant != "to_wall" && step.variant != "jubilife_stairs"){
                    return row.id + " " + step.name + ": walks from an unknown pose.";
                }
                const Walked walked = !at ? Walked{} : step.kind == StoryStepKind::ROUTE ? plan_route(*at, step)
                    : step.kind == StoryStepKind::LEGS ? walk_legs(*at, step) : step_on(*at, step);
                if (!walked.error.empty()){
                    return row.id + " " + walked.error;
                }
                at = walked.at;
            }else if (step.kind == StoryStepKind::GAME || step.kind == StoryStepKind::SHOP || step.kind == StoryStepKind::MASH){
                //  A scene or an event: where it ends is the table's to say.
                at.reset();
            }
            if (step.pose_after){
                at = step.pose_after;
            }
        }
        last = row.id;
    }
    return true;
}

//  With no reader for the game's language, the reads that used text give
//  way: a banner or save card that showed matches any map.
UnitTestResult test_no_text_reader(Logger&){
    for (Language language : {Language::Japanese, Language::Korean, Language::French}){
        if (story_reads_text(language)){
            return std::string("Expected no text reader for ") + language_data(language).name + ".";
        }
    }
    if (!location_banner_matches(UNREAD_LOCATION, "Route 202") || location_banner_matches("", "Route 202")){
        return "Expected a shown, unread banner to match and no banner not to.";
    }
    if (!story_reads_text(Language::English) || location_banner_matches("sandgemtown", "Route 202")){
        return "Expected English banners read and compared.";
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
    database.add<Test_StorySimulation>("00.00 to Floaroma, in order, each row saved first, English and Japanese", test_whole_table);
    database.add<Test_StorySimulation>("start and end checkpoints", test_start_and_end);
    database.add<Test_StorySimulation>("a blackout in 00.04b's training resets to its save", test_training_blackout);
    database.add<Test_StorySimulation>("a step that keeps failing stops after 3 attempts", test_gives_up_after_attempts);
    database.add<Test_StorySimulation>("each start pose is where the row before ends; routes plan on the maps", test_poses_chain);
    database.add<Test_StorySimulation>("no text reader for the game's language", test_no_text_reader);
}



}
}
}
