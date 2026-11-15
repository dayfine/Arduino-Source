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
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_BuiltinStoryActions.h"
#include "Pokemon_StoryStepExecutor.h"
#include "Pokemon_TilePose.h"
#include "Pokemon_StoryStepExecutor_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  A world of two maps. Routes arrive on their target; a step moves a tile
//  from the run's pose. Each step it does is written down as "KIND name",
//  with the pose it started from when that is known.
class FakeExecutor : public StoryStepExecutor{
public:
    std::vector<std::string> trace;
    std::set<std::string, std::less<>> conditions;
    //  Steps by name that always fail, and how.
    std::set<std::string> failing;
    //  Steps by name that fail this many more times, then pass.
    std::map<std::string, size_t> failures_left;
    //  The pose each step started from, by name.
    std::vector<std::pair<std::string, std::string>> poses_seen;
    //  The checkpoints begun, one per attempt.
    std::vector<std::string> attempts;
    //  Game steps by name that change nothing in the game.
    std::set<std::string> unchanged;

    virtual void begin_attempt(const StoryCheckpoint& data) override{
        attempts.push_back(data.id);
    }

    virtual Outcome run(const StoryStep& step, RunState& state) override{
        poses_seen.emplace_back(step.name, to_string(state.pose));
        trace.push_back(std::format("{} {}", story_step_kind_name(step.kind), step.name));
        if (failing.contains(step.name)){
            return outcome_failed("blocked", std::format("{} was blocked", step.name));
        }
        size_t& left = failures_left[step.name];
        if (left > 0){
            left--;
            return outcome_failed("blocked", std::format("{} was blocked", step.name));
        }
        return run_kind(step, state);
    }

    virtual bool holds(std::string_view condition, RunState& state) override{
        return conditions.contains(condition) || StoryStepExecutor::holds(condition, state);
    }

private:
    //  The kinds this world has; any other is unsupported.
    Outcome run_kind(const StoryStep& step, RunState& state){
        switch (step.kind){
        case StoryStepKind::ROUTE:
            return outcome_ok(pose_of(story_arg<GridPose>(step.args, "target")));
        case StoryStepKind::STEP:
            return step_one_tile(step, state);
        case StoryStepKind::TALK:
        case StoryStepKind::MASH:
            return outcome_ok();
        case StoryStepKind::BATTLE:
            //  The first battle is lost; a step that may lose goes on.
            return story_arg<bool>(step.args, "may_lose") ? outcome_ok() : outcome_failed("blackout");
        case StoryStepKind::SHOP:
            return shop(step);
        case StoryStepKind::GAME:
            return game(step, state);
        default:
            return unsupported_step(step);
        }
    }

    Outcome step_one_tile(const StoryStep& step, const RunState& state){
        const std::vector<TilePos> tiles = tiles_of(state.pose);
        if (tiles.size() != 1){
            return outcome_failed("no_pose", to_string(state.pose));
        }
        const Direction direction = story_arg<Direction>(step.args, "direction");
        return outcome_ok(tile_pose(state.pose.map, {tiles[0].next(direction)}, direction));
    }

    Outcome shop(const StoryStep& step){
        for (const StoryItem& item : story_arg<std::vector<StoryItem>>(step.args, "items")){
            trace.push_back(std::format("  bought {} {}", item.quantity, item.item));
        }
        return outcome_ok();
    }

    //  A game step sets a run flag named for its action.
    Outcome game(const StoryStep& step, RunState& state){
        trace.push_back(std::format("  game {}", step.action));
        state.flags.insert(step.action);
        return unchanged.contains(step.name) ? outcome_unchanged() : outcome_ok();
    }
};

StoryStep make(StoryStepKind kind, std::string name){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    //  As loading it would: the kind's built-in action's fallbacks.
    if (const StoryActionSpec* spec = find_story_action(builtin_story_actions(), kind, "")){
        step.args = with_fallbacks(*spec, {});
    }
    return step;
}

//  A grid pose on one tile.
GridPose grid(std::string map, TilePos tile, Direction facing){
    return GridPose{.map = std::move(map), .tiles = {tile}, .facing = facing};
}

//  A declared pose on one tile.
TileDeclared declared(std::string map, TilePos tile, Direction facing){
    return TileDeclared{.map = std::move(map), .tiles = {tile}, .facing = facing};
}

StoryStep route_to(std::string name, GridPose target){
    StoryStep step = make(StoryStepKind::ROUTE, std::move(name));
    step.args = {{"target", std::move(target)}, {"fight_wild", false}};
    return step;
}

StoryStep step_into(std::string name, Direction direction){
    StoryStep step = make(StoryStepKind::STEP, std::move(name));
    step.args = {{"direction", direction}};
    return step;
}

StoryStep scene(std::string name, std::optional<DeclaredPose> pose_after){
    StoryStep step = make(StoryStepKind::MASH, std::move(name));
    step.pose_after = std::move(pose_after);
    return step;
}

//  A lab checkpoint: to the table, a trigger, the scene into the lab, a
//  talk, a shop, a game step.
StoryCheckpoint lab_checkpoint(){
    StoryStep shop = make(StoryStepKind::SHOP, "potions");
    shop.args = {{"items", std::vector<StoryItem>{{.item = "potion", .quantity = 5}, {.item = "antidote", .quantity = 3}}}};
    StoryStep game = make(StoryStepKind::GAME, "dance");
    game.action = "dance";
    return StoryCheckpoint{
        .id = "00.03",
        .title = "to the lab",
        .start = declared("Town", {6, 8}, Direction::SOUTH),
        .steps = {
            route_to("below_trigger", grid("Town", {12, 2}, Direction::NORTH)),
            step_into("trigger", Direction::NORTH),
            scene("to_the_lab", declared("Lab", {6, 4}, Direction::NORTH)),
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

std::string joined(std::span<const std::string> lines){
    std::string text;
    for (const std::string& line : lines){
        text += std::format("\n  {}", line);
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
        return std::format("Expected the steps in table order:{}\nbut ran:{}", joined(expected), joined(executor.trace));
    }
    //  "from_save" is the engine's own event: the executor is not asked.
    const std::string first = event_log_line("00.03", 1, "from_save", "start");
    if (result.lines.empty() || std::find(result.lines.begin(), result.lines.end(), first) == result.lines.end()){
        return std::format("Expected the from_save event first:{}", joined(result.lines));
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
        return std::format("Expected one step skipped, got {}:{}", result.report.events_skipped, joined(result.lines));
    }
    if (std::find(executor.trace.begin(), executor.trace.end(), "ROUTE below_trigger") != executor.trace.end()){
        return std::format("Expected the route skipped:{}", joined(executor.trace));
    }
    if (std::find(executor.trace.begin(), executor.trace.end(), "TALK talk_oak") == executor.trace.end()){
        return std::format("Expected the talk run, one of its conditions not holding:{}", joined(executor.trace));
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
        return std::format("Expected the talk after the dance skipped on its flag:{}", joined(plain.trace));
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
            text += std::format("\n  {}: {}", item.first, item.second);
        }
        return std::format("Unexpected poses at the steps' starts:{}", text);
    }
    if (to_string(result.state.pose) != "Lab (6, 4) facing north (tile)"){
        return std::format("Expected the run to end at the scene's end, got {}", to_string(result.state.pose));
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
        return std::format("Expected a failure at event 3 trigger, blocked:{}", joined(result.lines));
    }
    if (executor.trace.size() != 2){
        return std::format("Expected nothing run after the failed step:{}", joined(executor.trace));
    }
    return true;
}

UnitTestResult test_unsupported_and_lost_battles(){
    //  The fake has no TRAIN, and loses battles a step may not lose.
    StoryStep train = make(StoryStepKind::TRAIN, "train");
    train.args = {{"battles", int64_t(3)}};
    FakeExecutor executor;
    Run result = run(StoryCheckpoint{.id = "01", .title = "", .start = std::nullopt, .steps = {train}}, executor);
    if (result.report.completed || result.report.failure.failure != "unsupported_step" || result.report.failure.detail != "TRAIN train"){
        return std::format("Expected TRAIN unsupported:{}", joined(result.lines));
    }
    StoryStep rival = make(StoryStepKind::BATTLE, "rival");
    rival.args = {{"may_lose", true}};
    StoryStep brock = make(StoryStepKind::BATTLE, "brock");
    result = run(StoryCheckpoint{.id = "02", .title = "", .start = std::nullopt, .steps = {rival, brock}}, executor);
    if (result.report.completed || result.report.failed_name != "brock" || result.report.failure.failure != "blackout"){
        return std::format("Expected the rival's battle passed and Brock's lost:{}", joined(result.lines));
    }
    return true;
}

//  An executor with pause points pauses before each of its checkpoint's steps,
//  labelled with the checkpoint and the step's place; a skipped step does not
//  pause, as it does not run.
UnitTestResult test_pause_points(){
    StoryCheckpoint data = lab_checkpoint();
    data.steps[1].skip_if = {"done"};
    std::vector<std::string> paused;
    FakeExecutor executor;
    executor.conditions = {"done"};
    StoryPausePoints pause(StoryBreakpoints::STEPS, [&paused](std::string_view label){
        paused.emplace_back(label);
    });
    executor.set_pause_points(&pause);
    const Run result = run(data, executor);
    const std::vector<std::string> expected{
        "00.03/1 below_trigger", "00.03/3 to_the_lab", "00.03/4 talk_oak", "00.03/5 potions", "00.03/6 dance",
    };
    if (paused != expected){
        return std::format("Expected a pause before each step that ran:{}{}", joined(paused), joined(result.lines));
    }
    FakeExecutor plain;
    const Run quiet = run(data, plain);
    return quiet.report.completed ? UnitTestResult(true) : UnitTestResult(joined(quiet.lines));
}


UnitTestResult test_lint(){
    StoryStep no_target = make(StoryStepKind::ROUTE, "walk");
    StoryStep again = make(StoryStepKind::TALK, "walk");
    StoryStep bad_legs = make(StoryStepKind::LEGS, "legs");
    bad_legs.args = {{"legs", std::vector<StoryLeg>{{.direction = Direction::NORTH, .tiles = 0}}}, {"fight_wild", false}};
    StoryStep no_variant = make(StoryStepKind::GAME, "");
    const StoryCheckpoint data{.id = "", .title = "", .start = std::nullopt, .steps = {no_target, again, bad_legs, no_variant}};
    const std::vector<std::string> expected{
        "a checkpoint has no id",
        " step 1 walk: ROUTE missing argument \"target\" (where to go; with several tiles, any of them)",
        " step 2: a second step named walk",
        " step 3 legs: LEGS a leg of no tiles",
        " step 4: no name",
        " step 4 : GAME no action",
    };
    const std::vector<std::string> problems = lint_story_checkpoint(data);
    if (problems != expected){
        return std::format("Unexpected lint:{}", joined(problems));
    }
    if (!lint_story_checkpoint(lab_checkpoint()).empty()){
        return std::format("Expected the lab checkpoint to lint clean:{}", joined(lint_story_checkpoint(lab_checkpoint())));
    }
    return true;
}


struct StoryStepTestCase{
    std::string name;
    struct Input{
        UnitTestResult (*test)();
    };

    struct Expectation{
        std::string_view description;
    };

    Input input;
    Expectation expectation;
};

class Test_StoryStepExecutor : public UnitTest{
public:
    explicit Test_StoryStepExecutor(StoryStepTestCase scenario)
        : UnitTest(std::format("Pokemon::StoryStepExecutor - {}", scenario.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.test();
    }

private:
    StoryStepTestCase m_scenario;
};

const StoryStepTestCase STORY_STEP_CASES[] = {
    {
        .name = "steps run in table order", .input = {.test = test_events_in_table_order},
        .expectation = {.description = "executes story steps sequentially as defined in table"},
    },
    {
        .name = "skip_if asks the executor", .input = {.test = test_skip_if},
        .expectation = {.description = "delegates skip condition check to executor"},
    },
    {
        .name = "each step starts where the last left the player", .input = {.test = test_pose_propagation},
        .expectation = {.description = "propagates player pose across sequential step executions"},
    },
    {
        .name = "a failure names its step", .input = {.test = test_failure_names_the_step},
        .expectation = {.description = "records failing step identifier on error"},
    },
    {
        .name = "unsupported kinds and lost battles", .input = {.test = test_unsupported_and_lost_battles},
        .expectation = {.description = "flags unsupported step types and handles battle defeat"},
    },
    {
        .name = "pause points pause before each step", .input = {.test = test_pause_points},
        .expectation = {.description = "calls the game's pause points before each step that runs"},
    },
    {
        .name = "lint", .input = {.test = test_lint},
        .expectation = {.description = "verifies lint validation rules on story checkpoints"},
    },
};

} // namespace


void add_tests_StoryStepExecutor(UnitTestDatabase& database){
    for (const auto& scenario : STORY_STEP_CASES){
        database.add<Test_StoryStepExecutor>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
