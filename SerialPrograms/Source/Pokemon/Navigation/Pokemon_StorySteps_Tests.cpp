/*  Story Steps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Story tables built into checkpoints and run against a fake executor
 *  that writes down each step it is asked to do: the order of the events,
 *  skipped steps, the pose each step leaves, and how a failure is named.
 *  Nothing here belongs to one game.
 *
 */

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_StorySegments.h"
#include "Pokemon_StorySteps.h"
#include "Pokemon_TilePose.h"
#include "Pokemon_Navigation_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  A world of two maps. Routes arrive on their target; a step moves a tile
//  from the run's pose. Each step it does is written down as "KIND name",
//  with the pose it started from when that is known.
class FakeExecutor : public StoryStepExecutor{
public:
    std::vector<std::string> trace;
    std::set<std::string> conditions;
    //  Steps by name that always fail, and how.
    std::set<std::string> failing;
    //  Steps by name that fail this many more times, then pass.
    std::map<std::string, size_t> failures_left;
    //  The pose each step started from, by name.
    std::vector<std::pair<std::string, std::string>> poses_seen;
    //  The checkpoints begun, one per attempt.
    std::vector<std::string> attempts;

    virtual void begin_attempt(const StoryCheckpoint& data) override{
        attempts.push_back(data.id);
    }

    virtual Outcome run(RunState& state, const StoryStep& step) override{
        poses_seen.emplace_back(step.name, to_string(state.pose));
        trace.push_back(std::string(story_step_kind_name(step.kind)) + " " + step.name);
        if (failing.contains(step.name)){
            return outcome_failed("blocked", step.name + " was blocked");
        }
        size_t& left = failures_left[step.name];
        if (left > 0){
            left--;
            return outcome_failed("blocked", step.name + " was blocked");
        }
        return StoryStepExecutor::run(state, step);
    }
    virtual bool holds(const std::string& condition, RunState& state) override{
        return conditions.contains(condition) || StoryStepExecutor::holds(condition, state);
    }
    virtual Outcome route(RunState&, const StoryStep& step) override{
        return outcome_ok(*step.target);
    }
    virtual Outcome step(RunState& state, const StoryStep& step) override{
        const std::vector<TilePos> tiles = tiles_of(state.pose);
        if (tiles.size() != 1){
            return outcome_failed("no_pose", to_string(state.pose));
        }
        return outcome_ok(tile_pose(state.pose.map, {tiles[0].next(step.direction)}, step.direction));
    }
    virtual Outcome talk(RunState&, const StoryStep&) override{
        return outcome_ok();
    }
    virtual Outcome mash(RunState&, const StoryStep&) override{
        return outcome_ok();
    }
    virtual Outcome battle(RunState&, const StoryStep& step) override{
        //  The first battle is lost; a step that may lose goes on.
        return step.may_lose ? outcome_ok() : outcome_failed("blackout");
    }
    virtual Outcome shop(RunState&, const StoryStep& step) override{
        for (const StoryItem& item : step.items){
            trace.push_back("  bought " + std::to_string(item.quantity) + " " + item.item);
        }
        return outcome_ok();
    }
    //  A game step sets a run flag named for its variant.
    virtual Outcome game(RunState& state, const StoryStep& step) override{
        trace.push_back("  game " + step.variant);
        state.flags.insert(step.variant);
        return outcome_ok();
    }
};

StoryStep make(StoryStepKind kind, std::string name){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    return step;
}

StoryStep route_to(std::string name, Pose target){
    StoryStep step = make(StoryStepKind::ROUTE, std::move(name));
    step.target = std::move(target);
    return step;
}

StoryStep step_into(std::string name, Direction direction){
    StoryStep step = make(StoryStepKind::STEP, std::move(name));
    step.direction = direction;
    return step;
}

StoryStep scene(std::string name, std::optional<Pose> pose_after){
    StoryStep step = make(StoryStepKind::MASH, std::move(name));
    step.pose_after = std::move(pose_after);
    return step;
}

//  A lab checkpoint: to the table, a trigger, the scene into the lab, a
//  talk, a shop, a game step.
StoryCheckpoint lab_checkpoint(){
    StoryStep shop = make(StoryStepKind::SHOP, "potions");
    shop.items = {{.item = "potion", .quantity = 5}, {.item = "antidote", .quantity = 3}};
    StoryStep game = make(StoryStepKind::GAME, "dance");
    game.variant = "dance";
    return StoryCheckpoint{
        .id = "00.03",
        .title = "to the lab",
        .start = tile_pose("Town", {{6, 8}}, Direction::SOUTH),
        .steps = {
            route_to("below_trigger", tile_pose("Town", {{12, 2}}, Direction::NORTH)),
            step_into("trigger", Direction::NORTH),
            scene("to_the_lab", tile_pose("Lab", {{6, 4}}, Direction::NORTH)),
            make(StoryStepKind::TALK, "talk_oak"),
            shop,
            game,
        },
    };
}

struct Run{
    CheckpointReport report;
    RunState state;
    std::vector<std::string> lines;
};

Run run(const StoryCheckpoint& data, StoryStepExecutor& executor){
    Run ret;
    Checkpoint checkpoint = build_story_checkpoint(data, executor);
    EventLogger log = [&ret](const std::string& line){ ret.lines.push_back(line); };
    ret.report = run_checkpoint(ret.state, checkpoint, log);
    return ret;
}

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n  " + line;
    }
    return text;
}


UnitTestResult test_events_in_table_order(){
    const StoryCheckpoint data = lab_checkpoint();
    FakeExecutor executor;
    const Run result = run(data, executor);
    const std::vector<std::string> expected{
        "ROUTE below_trigger",
        "STEP trigger",
        "MASH to_the_lab",
        "TALK talk_oak",
        "SHOP potions",
        "  bought 5 potion",
        "  bought 3 antidote",
        "GAME dance",
        "  game dance",
    };
    if (!result.report.completed || executor.trace != expected){
        return "Expected the steps in table order:" + joined(expected) + "\nbut ran:" + joined(executor.trace);
    }
    //  "from_save" is the engine's own event: the executor is not asked.
    const std::string first = event_log_line("00.03", 1, "from_save", "start");
    if (result.lines.empty() || std::find(result.lines.begin(), result.lines.end(), first) == result.lines.end()){
        return "Expected the from_save event first:" + joined(result.lines);
    }
    return true;
}

UnitTestResult test_skip_if(){
    StoryCheckpoint data = lab_checkpoint();
    data.steps[0].skip_if = {"has_parcel"};
    data.steps[3].skip_if = {"has_parcel", "talked"};
    FakeExecutor executor;
    executor.conditions = {"has_parcel"};
    //  The route is skipped; the talk needs both conditions, so it runs.
    const Run result = run(data, executor);
    if (!result.report.completed || result.report.events_skipped != 1){
        return "Expected one step skipped, got " + std::to_string(result.report.events_skipped) + ":" + joined(result.lines);
    }
    if (std::find(executor.trace.begin(), executor.trace.end(), "ROUTE below_trigger") != executor.trace.end()){
        return "Expected the route skipped:" + joined(executor.trace);
    }
    if (std::find(executor.trace.begin(), executor.trace.end(), "TALK talk_oak") == executor.trace.end()){
        return "Expected the talk run, one of its conditions not holding:" + joined(executor.trace);
    }

    //  By default a condition is a run flag: the game step "dance" sets
    //  one, and a talk after it that needs it is skipped.
    StoryStep after = make(StoryStepKind::TALK, "talk_after_dance");
    after.skip_if = {"dance"};
    data = lab_checkpoint();
    data.steps.push_back(after);
    FakeExecutor plain;
    const Run flagged = run(data, plain);
    if (!flagged.report.completed || flagged.report.events_skipped != 1 || plain.trace.back() != "  game dance"){
        return "Expected the talk after the dance skipped on its flag:" + joined(plain.trace);
    }
    return true;
}

UnitTestResult test_pose_propagation(){
    const StoryCheckpoint data = lab_checkpoint();
    FakeExecutor executor;
    const Run result = run(data, executor);
    //  Each step starts where the one before left the player: the save, the
    //  route's target, a tile north of it, the scene's end, and the scene's
    //  end again after a talk that says nothing of where it leaves us.
    const std::vector<std::pair<std::string, std::string>> expected{
        {"below_trigger", "Town (6, 8) facing south (tile)"},
        {"trigger", "Town (12, 2) facing north (tile)"},
        {"to_the_lab", "Town (12, 1) facing north (tile)"},
        {"talk_oak", "Lab (6, 4) facing north (tile)"},
        {"potions", "Lab (6, 4) facing north (tile)"},
        {"dance", "Lab (6, 4) facing north (tile)"},
    };
    if (!result.report.completed || executor.poses_seen != expected){
        std::string text;
        for (const auto& item : executor.poses_seen){
            text += "\n  " + item.first + ": " + item.second;
        }
        return "Unexpected poses at the steps' starts:" + text;
    }
    if (to_string(result.state.pose) != "Lab (6, 4) facing north (tile)"){
        return "Expected the run to end at the scene's end, got " + to_string(result.state.pose);
    }
    return true;
}

UnitTestResult test_failure_names_the_step(){
    const StoryCheckpoint data = lab_checkpoint();
    FakeExecutor executor;
    executor.failing = {"trigger"};
    const Run result = run(data, executor);
    //  Event 3: from_save is event 1.
    if (result.report.completed || result.report.failed_event != 3 || result.report.failed_name != "trigger"
        || result.report.failure.failure != "blocked"
    ){
        return "Expected a failure at event 3 trigger, blocked:" + joined(result.lines);
    }
    if (executor.trace.size() != 2){
        return "Expected nothing run after the failed step:" + joined(executor.trace);
    }
    return true;
}

UnitTestResult test_unsupported_and_lost_battles(){
    //  The fake has no TRAIN, and loses battles a step may not lose.
    StoryStep train = make(StoryStepKind::TRAIN, "train");
    train.battles = 3;
    FakeExecutor executor;
    Run result = run(StoryCheckpoint{.id = "01", .title = "", .start = std::nullopt, .steps = {train}}, executor);
    if (result.report.completed || result.report.failure.failure != "unsupported_step" || result.report.failure.detail != "TRAIN train"){
        return "Expected TRAIN unsupported:" + joined(result.lines);
    }
    StoryStep rival = make(StoryStepKind::BATTLE, "rival");
    rival.may_lose = true;
    StoryStep brock = make(StoryStepKind::BATTLE, "brock");
    result = run(StoryCheckpoint{.id = "02", .title = "", .start = std::nullopt, .steps = {rival, brock}}, executor);
    if (result.report.completed || result.report.failed_name != "brock" || result.report.failure.failure != "blackout"){
        return "Expected the rival's battle passed and Brock's lost:" + joined(result.lines);
    }
    return true;
}

UnitTestResult test_segment_of_tables(){
    //  Two rows built into a segment, run with saves and a reload from the
    //  game's hooks: the second row's first attempt fails, its retry starts
    //  over from its start pose.
    const std::vector<StoryCheckpoint> tables{
        lab_checkpoint(),
        StoryCheckpoint{
            .id = "00.04",
            .title = "the parcel",
            .start = tile_pose("Lab", {{6, 4}}, Direction::NORTH),
            .steps = {make(StoryStepKind::TALK, "give_parcel")},
        },
    };
    FakeExecutor executor;
    Segment segment;
    segment.id = "00";
    for (const StoryCheckpoint& data : tables){
        segment.checkpoints.push_back(CheckpointSpec{
            .id = data.id, .title = data.title, .saves = true,
            .build = [&data, &executor]{ return build_story_checkpoint(data, executor); },
        });
    }
    std::vector<std::string> lines;
    SegmentHooks hooks;
    hooks.save = [&executor](RunState&){
        executor.trace.push_back("save");
        return outcome_ok();
    };
    //  Whatever blocked the talk has gone by the reload.
    hooks.reload = [&executor](RunState&){
        executor.trace.push_back("reload");
        executor.failing.clear();
        return outcome_ok();
    };
    hooks.log = [&lines](const std::string& line){ lines.push_back(line); };
    executor.failing = {"give_parcel"};
    RunState state;
    const SegmentReport report = run_segment(state, segment, SegmentRange{}, SegmentPolicy{}, hooks);
    const std::vector<std::string> tail{"save", "TALK give_parcel", "reload", "TALK give_parcel", "save"};
    const bool tail_matches = executor.trace.size() >= tail.size()
        && std::equal(tail.begin(), tail.end(), executor.trace.end() - (std::ptrdiff_t)tail.size());
    if (!report.completed || report.reloads != 1 || !tail_matches){
        return "Expected 00.04 reloaded once and passed:" + joined(executor.trace) + joined(lines);
    }
    //  Each attempt is begun anew: 00.03 once, 00.04 twice.
    if (executor.attempts != std::vector<std::string>{"00.03", "00.04", "00.04"}){
        return "Expected the executor told of each attempt:" + joined(executor.attempts);
    }
    //  The report and the log tell a one-shot pass from a retried one.
    const bool runs_ok = report.runs.size() == 2
        && report.runs[0].one_shot() && report.runs[0].attempts == 1
        && !report.runs[1].one_shot() && report.runs[1].attempts == 2 && report.runs[1].passed
        && report.one_shot_passes() == 1;
    const auto has = [&lines](const std::string& line){
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    };
    if (!runs_ok
        || !has("[Checkpoint] 00.03: passed on attempt 1 (one-shot)")
        || !has("[Checkpoint] 00.04: passed on attempt 2 (1 reset)")
        || !has("[Segment] 00: 1 of 2 checkpoints one-shot")
    ){
        return "Expected 00.03 one-shot and 00.04 passed on attempt 2:" + joined(lines);
    }
    return true;
}


//  The two tables of test_segment_of_tables() as a segment run by `executor`,
//  with hooks that write saves, reloads and recoveries into its trace.
struct PolicyLab{
    std::vector<StoryCheckpoint> tables{
        lab_checkpoint(),
        StoryCheckpoint{
            .id = "00.04",
            .title = "the parcel",
            .start = tile_pose("Lab", {{6, 4}}, Direction::NORTH),
            .steps = {make(StoryStepKind::TALK, "give_parcel")},
        },
    };
    FakeExecutor executor;
    std::vector<std::string> lines;

    SegmentReport run(const SegmentPolicy& policy){
        Segment segment;
        segment.id = "00";
        for (const StoryCheckpoint& data : tables){
            segment.checkpoints.push_back(CheckpointSpec{
                .id = data.id, .title = data.title, .saves = true,
                .build = [&data, this]{ return build_story_checkpoint(data, executor); },
            });
        }
        SegmentHooks hooks;
        hooks.save = [this](RunState&){
            executor.trace.push_back("save");
            return outcome_ok();
        };
        hooks.reload = [this](RunState&){
            executor.trace.push_back("reload");
            return outcome_ok();
        };
        hooks.recover = [this](RunState&, const std::string& event){
            executor.trace.push_back("recover " + event);
            return outcome_ok();
        };
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        RunState state;
        return run_segment(state, segment, SegmentRange{}, policy, hooks);
    }
    bool has(const std::string& line) const{
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    }
    size_t count(const std::string& item) const{
        return (size_t)std::count(executor.trace.begin(), executor.trace.end(), item);
    }
};

UnitTestResult test_policy_attempts_then_fail(){
    //  Three attempts by default, then the segment stops on the last one's failure.
    PolicyLab lab;
    lab.executor.failing = {"give_parcel"};
    const SegmentReport report = lab.run(SegmentPolicy{});
    if (report.completed || report.reloads != 2 || lab.count("TALK give_parcel") != 3
        || report.runs.size() != 2 || report.runs[1].passed || report.runs[1].attempts != 3
        || report.failed_checkpoint != "00.04" || report.failure.failure != "blocked"
    ){
        return "Expected 00.04 tried 3 times, then failed:" + joined(lab.executor.trace) + joined(lab.lines);
    }
    return true;
}

UnitTestResult test_policy_stop_at_first_failure(){
    PolicyLab lab;
    lab.executor.failing = {"give_parcel"};
    const SegmentReport report = lab.run(SegmentPolicy{.stop_at_first_failure = true});
    if (report.completed || !report.stopped_at_first_failure || report.reloads != 0 || lab.count("reload") != 0
        || lab.count("TALK give_parcel") != 1
        || !lab.has("[Segment] 00: 00.04 failed; stopping at the first failure, the game stays where it failed")
    ){
        return "Expected a stop at the first failure, with no reload:" + joined(lab.executor.trace) + joined(lab.lines);
    }
    return true;
}

UnitTestResult test_policy_recovers_in_place(){
    //  The talk fails twice; two recoveries in place get it through
    //  without a reload, but not one-shot.
    PolicyLab lab;
    lab.executor.failures_left = {{"give_parcel", 2}};
    const SegmentReport report = lab.run(SegmentPolicy{.recoveries_per_event = 2});
    if (!report.completed || report.reloads != 0 || lab.count("recover give_parcel") != 2
        || report.runs.size() != 2 || report.runs[1].recoveries != 2 || report.runs[1].one_shot()
        || !lab.has("[Checkpoint] 00.04: passed on attempt 1 (one-shot), 2 in place")
    ){
        return "Expected 00.04 recovered in place twice:" + joined(lab.executor.trace) + joined(lab.lines);
    }
    //  Past the budget, the checkpoint is reloaded as before.
    PolicyLab reloaded;
    reloaded.executor.failures_left = {{"give_parcel", 2}};
    const SegmentReport second = reloaded.run(SegmentPolicy{.recoveries_per_event = 1});
    if (!second.completed || second.reloads != 1 || second.runs[1].attempts != 2){
        return "Expected a reload once the in-place budget was used:" + joined(reloaded.executor.trace) + joined(reloaded.lines);
    }
    return true;
}

//  The checks level: THOROUGH by default, and what the program sets is
//  what a step reads.
UnitTestResult test_checks_level(){
    class CheckingExecutor : public FakeExecutor{
    public:
        std::vector<std::string> seen;
        virtual Outcome talk(RunState&, const StoryStep& step) override{
            seen.push_back(step.name + " " + story_checks_name(checks()));
            return outcome_ok();
        }
    };
    StoryCheckpoint data = lab_checkpoint();
    CheckingExecutor executor;
    run(data, executor);
    executor.set_checks(StoryChecks::FAST);
    run(data, executor);
    if (executor.seen != std::vector<std::string>{"talk_oak thorough", "talk_oak fast"}){
        return "Expected the talk to read thorough, then fast:" + joined(executor.seen);
    }
    return true;
}

//  The program's option group: 3 attempts, no stop, Thorough, by default;
//  Fast when chosen.
UnitTestResult test_run_options(){
    StoryRunOptions options;
    const SegmentPolicy policy = options.policy();
    if (policy.attempts_per_checkpoint != 3 || policy.stop_at_first_failure || options.checks() != StoryChecks::THOROUGH){
        return "Expected 3 attempts, no stop at the first failure, and Thorough checks by default.";
    }
    options.CHECKS.set(StoryChecks::FAST);
    if (options.checks() != StoryChecks::FAST){
        return "Expected Fast checks once chosen.";
    }
    return true;
}

UnitTestResult test_lint(){
    StoryStep no_target = make(StoryStepKind::ROUTE, "walk");
    StoryStep again = make(StoryStepKind::TALK, "walk");
    StoryStep bad_legs = make(StoryStepKind::LEGS, "legs");
    bad_legs.legs = {{.direction = Direction::NORTH, .tiles = 0}};
    StoryStep no_variant = make(StoryStepKind::GAME, "");
    const StoryCheckpoint data{.id = "", .title = "", .start = std::nullopt, .steps = {no_target, again, bad_legs, no_variant}};
    const std::vector<std::string> expected{
        "a checkpoint has no id",
        " step 1 walk: ROUTE no target",
        " step 2: a second step named walk",
        " step 3 legs: LEGS a leg of no tiles",
        " step 4: no name",
        " step 4 : GAME no variant",
    };
    const std::vector<std::string> problems = lint_story_checkpoint(data);
    if (problems != expected){
        return "Unexpected lint:" + joined(problems);
    }
    if (!lint_story_checkpoint(lab_checkpoint()).empty()){
        return "Expected the lab checkpoint to lint clean:" + joined(lint_story_checkpoint(lab_checkpoint()));
    }
    return true;
}


class Test_StorySteps : public UnitTest{
public:
    Test_StorySteps(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("Pokemon::StorySteps - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

}


void add_tests_StorySteps(UnitTestDatabase& database){
    database.add<Test_StorySteps>("steps run in table order", test_events_in_table_order);
    database.add<Test_StorySteps>("skip_if asks the executor", test_skip_if);
    database.add<Test_StorySteps>("each step starts where the last left the player", test_pose_propagation);
    database.add<Test_StorySteps>("a failure names its step", test_failure_names_the_step);
    database.add<Test_StorySteps>("unsupported kinds and lost battles", test_unsupported_and_lost_battles);
    database.add<Test_StorySteps>("tables as a segment, with a reload", test_segment_of_tables);
    database.add<Test_StorySteps>("lint", test_lint);
    database.add<Test_StorySteps>("the checks level reaches the steps", test_checks_level);
    database.add<Test_StorySteps>("the run options' defaults", test_run_options);
    database.add<Test_StorySteps>("policy: attempts per checkpoint, then fail", test_policy_attempts_then_fail);
    database.add<Test_StorySteps>("policy: stop at the first failure", test_policy_stop_at_first_failure);
    database.add<Test_StorySteps>("policy: recover in place before reloading", test_policy_recovers_in_place);
}



}
}
