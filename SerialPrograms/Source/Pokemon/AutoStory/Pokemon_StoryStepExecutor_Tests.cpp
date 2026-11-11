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
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "Pokemon_AutoStorySelect.h"
#include "Pokemon_AutoStoryStats.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_BuiltinStoryActions.h"
#include "Pokemon_StorySegments.h"
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

//  Runs `tables` as one segment whose saves and reloads go into the
//  executor's trace and whose log lines go into `lines`. Whatever blocked a
//  step has gone by the reload.
SegmentReport run_tables(std::span<const StoryCheckpoint> tables, FakeExecutor& executor, std::vector<std::string>& lines){
    Segment segment;
    segment.id = "00";
    for (const StoryCheckpoint& data : tables){
        segment.checkpoints.push_back(CheckpointSpec{
            .id = data.id, .title = data.title, .saves = true,
            .build = [&data, &executor]{ return build_story_checkpoint(data, executor); },
        });
    }
    SegmentHooks hooks;
    hooks.save = [&executor](RunState&){
        executor.trace.push_back("save");
        return outcome_ok();
    };
    hooks.reload = [&executor](RunState&){
        executor.trace.push_back("reload");
        executor.failing.clear();
        return outcome_ok();
    };
    hooks.log = [&lines](const std::string& line){ lines.push_back(line); };
    RunState state;
    return run_segment(SegmentRange{}, SegmentPolicy{}, segment, hooks, state);
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
            .start = declared("Lab", {6, 4}, Direction::NORTH),
            .steps = {make(StoryStepKind::TALK, "give_parcel")},
        },
    };
    FakeExecutor executor;
    executor.failing = {"give_parcel"};
    std::vector<std::string> lines;
    const SegmentReport report = run_tables(tables, executor, lines);
    const std::vector<std::string> tail{"save", "TALK give_parcel", "reload", "TALK give_parcel", "save"};
    const bool tail_matches = executor.trace.size() >= tail.size()
        && std::equal(tail.begin(), tail.end(), executor.trace.end() - (std::ptrdiff_t)tail.size());
    if (!report.completed || report.reloads != 1 || !tail_matches){
        return std::format("Expected 00.04 reloaded once and passed:{}{}", joined(executor.trace), joined(lines));
    }
    //  Each attempt is begun anew: 00.03 once, 00.04 twice.
    if (executor.attempts != std::vector<std::string>{"00.03", "00.04", "00.04"}){
        return std::format("Expected the executor told of each attempt:{}", joined(executor.attempts));
    }
    //  The report and the log tell a one-shot pass from a retried one.
    const bool runs_ok = report.runs.size() == 2
        && report.runs[0].one_shot() && report.runs[0].attempts == 1
        && !report.runs[1].one_shot() && report.runs[1].attempts == 2 && report.runs[1].passed
        && report.one_shot_passes() == 1;
    const auto has = [&lines](std::string_view line){
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    };
    if (!runs_ok
        || !has("[Checkpoint] 00.03: passed on attempt 1 (one-shot)")
        || !has("[Checkpoint] 00.04: passed on attempt 2 (1 reset)")
        || !has("[Segment] 00: 1 of 2 checkpoints one-shot")
    ){
        return std::format("Expected 00.03 one-shot and 00.04 passed on attempt 2:{}", joined(lines));
    }
    return true;
}


//  The two tables of test_segment_of_tables() as a segment run by `executor`,
//  with hooks that write saves and reloads into its trace.
struct PolicyLab{
    std::vector<StoryCheckpoint> tables{
        lab_checkpoint(),
        StoryCheckpoint{
            .id = "00.04",
            .title = "the parcel",
            .start = declared("Lab", {6, 4}, Direction::NORTH),
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
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        RunState state;
        return run_segment(SegmentRange{}, policy, segment, hooks, state);
    }
    bool has(std::string_view line) const{
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    }
    size_t count(std::string_view item) const{
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
        return std::format("Expected 00.04 tried 3 times, then failed:{}{}", joined(lab.executor.trace), joined(lab.lines));
    }
    return true;
}

UnitTestResult test_policy_stop_at_first_failure(){
    PolicyLab lab;
    lab.executor.failing = {"give_parcel"};
    const SegmentReport report = lab.run(SegmentPolicy{.attempts_per_checkpoint = 1});
    if (report.completed || report.reloads != 0 || lab.count("reload") != 0
        || lab.count("TALK give_parcel") != 1
    ){
        return std::format("Expected a stop at the first failure, with no reload:{}{}", joined(lab.executor.trace), joined(lab.lines));
    }
    return true;
}

//  A boss's checkpoint: a TEACH of a move the lead knows (changes
//  nothing), a TEACH that teaches, both once per save, then the battle,
//  lost on the first attempt. The segment's saves empty `prepared`.
struct BossLab{
    StoryCheckpoint table{
        .id = "00.15b",
        .title = "the rival",
        .start = declared("Cerulean", {23, 7}, Direction::NORTH),
        .steps = {once("teach_known"), once("teach_new"), make(StoryStepKind::BATTLE, "rival")},
    };
    FakeExecutor executor;
    PreparedSteps prepared;
    std::vector<std::string> lines;

    static StoryStep once(std::string name){
        StoryStep step = make(StoryStepKind::GAME, name);
        step.action = name;
        step.once_per_save = true;
        return step;
    }
    BossLab(){
        executor.unchanged = {"teach_known"};
    }
    Segment segment(){
        Segment segment;
        segment.id = "00";
        segment.checkpoints.push_back(CheckpointSpec{
            .id = table.id, .title = table.title, .saves = true,
            .build = [this]{ return build_story_checkpoint(table, executor, &prepared); },
        });
        return segment;
    }
    SegmentHooks hooks(){
        SegmentHooks hooks;
        hooks.save = [this](RunState&){
            executor.trace.push_back("save");
            prepared.clear();
            return outcome_ok();
        };
        hooks.reload = [this](RunState&){
            executor.trace.push_back("reload");
            return outcome_ok();
        };
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        return hooks;
    }
    size_t count(std::string_view item) const{
        return (size_t)std::count(executor.trace.begin(), executor.trace.end(), item);
    }
};

//  A retry after a reload of the same save skips the preparation that
//  changed nothing and goes to the battle; the TEACH that taught runs
//  again, as the reload undid it. The save at the end empties the memory.
UnitTestResult test_once_per_save_retry(){
    BossLab lab;
    StoryStep battle = make(StoryStepKind::BATTLE, "rival");
    battle.args = {{"may_lose", true}};
    lab.table.steps.back() = battle;
    lab.executor.failures_left = {{"rival", 1}};
    Segment segment = lab.segment();
    SegmentHooks hooks = lab.hooks();
    RunState state;
    const SegmentReport report = run_segment(SegmentRange{}, SegmentPolicy{}, segment, hooks, state);
    if (!report.completed || report.reloads != 1 || lab.count("GAME teach_known") != 1
        || lab.count("GAME teach_new") != 2 || lab.count("BATTLE rival") != 2
    ){
        return std::format("Expected the known TEACH once, the new one twice, the battle twice:{}{}", joined(lab.executor.trace), joined(lab.lines));
    }
    if (!lab.prepared.empty()){
        return "Expected the save to empty the prepared steps.";
    }
    return true;
}

//  Verify mode: three runs from one save; the preparation that changed
//  nothing runs in the first only.
UnitTestResult test_once_per_save_verify(){
    BossLab lab;
    StoryStep battle = make(StoryStepKind::BATTLE, "rival");
    battle.args = {{"may_lose", true}};
    lab.table.steps.back() = battle;
    Segment segment = lab.segment();
    SegmentHooks hooks = lab.hooks();
    RunState state;
    const VerifyReport report = verify_checkpoint("00.15b", VerifyPolicy{}, segment, hooks, state);
    if (!report.verified || report.runs != 3 || lab.count("GAME teach_known") != 1
        || lab.count("GAME teach_new") != 3 || lab.count("BATTLE rival") != 3
    ){
        return std::format("Expected the known TEACH once in 3 runs, the new one each run:{}{}", joined(lab.executor.trace), joined(lab.lines));
    }
    //  Without the memory, every run prepares.
    FakeExecutor plain;
    plain.unchanged = {"teach_known"};
    Segment without;
    without.id = "00";
    without.checkpoints.push_back(CheckpointSpec{
        .id = lab.table.id, .title = "", .saves = true,
        .build = [&]{ return build_story_checkpoint(lab.table, plain); },
    });
    RunState again;
    verify_checkpoint("00.15b", VerifyPolicy{}, without, hooks, again);
    if (std::count(plain.trace.begin(), plain.trace.end(), "GAME teach_known") != 3){
        return std::format("Expected every run to prepare without PreparedSteps:{}", joined(plain.trace));
    }
    return true;
}


//  Two sections of a story, as BDSP has segments 00 and 01.
const std::vector<StoryCheckpoint>& two_sections(){
    static const std::vector<StoryCheckpoint> rows{
        StoryCheckpoint{.id = "00.01", .title = "Leave the house", .start = std::nullopt, .steps = {}},
        StoryCheckpoint{.id = "00.02", .title = "Walk to Sandgem Town", .start = std::nullopt, .steps = {}},
        StoryCheckpoint{.id = "01.01", .title = "Jubilife to Barry", .start = std::nullopt, .steps = {}},
    };
    return rows;
}

const StringSelectDatabase& two_sections_database(){
    static const StringSelectDatabase database = []{
        std::vector<std::string> labels;
        for (const StoryCheckpoint& checkpoint : two_sections()){
            labels.push_back(story_checkpoint_label(checkpoint));
        }
        return story_checkpoint_database(labels);
    }();
    return database;
}

//  The dropdowns list the checkpoints as "id title", slugged by their index
//  in the whole story, which is story_index() of a section's own index.
UnitTestResult test_checkpoint_dropdowns(){
    const StringSelectDatabase& database = two_sections_database();
    const std::vector<std::string> expected{"00.01 Leave the house", "00.02 Walk to Sandgem Town", "01.01 Jubilife to Barry"};
    if (database.size() != expected.size()){
        return std::format("Expected 3 entries, got {}", database.size());
    }
    for (size_t c = 0; c < expected.size(); c++){
        if (database[c].display_name != expected[c] || database[c].slug != std::to_string(c)){
            return std::format("Unexpected entry {}: {} {}", c, database[c].slug, database[c].display_name);
        }
    }
    if (database[story_index(true, 0, 2)].display_name != "01.01 Jubilife to Barry"){
        return "Expected the second section's first entry at story_index(true, 0, 2).";
    }
    if (story_checkpoint_label(StoryCheckpoint{.id = "00.09", .title = "Pewter Gym: Camper Liam, then Brock", .start = std::nullopt, .steps = {}})
        != "00.09 Pewter Gym: Camper Liam, then Brock"
    ){
        return "Unexpected label.";
    }
    StoryRunOptions options(database, Gen8ToGen9Languages_Database());
    if (options.start_index() != 0 || options.end_index() != 2 || !options.check_validity().empty()){
        return "Expected the whole story by default.";
    }
    options.START_CHECKPOINT.set_by_index(2);
    options.END_CHECKPOINT.set_by_index(1);
    if (options.check_validity().empty()){
        return "Expected a start after the end to be refused.";
    }
    return true;
}

//  A segment's report in the shared AutoStory stats.
UnitTestResult test_story_stats(){
    SegmentReport report;
    report.reloads = 2;
    report.runs = {
        CheckpointRun{.id = "00.03", .passed = true, .attempts = 1},
        CheckpointRun{.id = "00.04", .passed = true, .attempts = 3},
        CheckpointRun{.id = "00.05", .passed = false, .attempts = 1},
    };
    AutoStoryStats stats;
    record_story_counts(report.story_counts(), stats);
    if (stats.m_segment != 1 || stats.m_checkpoint != 2 || stats.m_reset != 2){
        return "Expected 1 segment, 2 checkpoints and 2 resets.";
    }
    return true;
}

//  The program's option group: 3 attempts, no stop, Thorough, by default;
//  Fast when chosen.
UnitTestResult test_run_options(){
    StoryRunOptions options(two_sections_database(), Gen8ToGen9Languages_Database());
    const SegmentPolicy policy = options.policy();
    if (policy.attempts_per_checkpoint != 3 || options.check_level() != StoryCheckLevel::DEFAULT){
        return "Expected 3 attempts, no stop at the first failure, and Thorough checks by default.";
    }
    options.CHECKS.set(StoryCheckLevel::FAST);
    if (options.check_level() != StoryCheckLevel::FAST){
        return "Expected Fast checks once chosen.";
    }
    //  Every game language can be chosen, read or not.
    if (options.language() != Language::English){
        return "Expected English by default.";
    }
    options.LANGUAGE.set(Language::Japanese);
    if (options.language() != Language::Japanese || !options.check_validity().empty()){
        return "Expected Japanese to be chosen and valid, reader or not.";
    }
    return true;
}

//  Verify mode: off by default; 3 runs and 2 passes when on, for one
//  checkpoint only, never needing more passes than runs.
UnitTestResult test_verify_options(){
    StoryRunOptions options(two_sections_database(), Gen8ToGen9Languages_Database());
    if (options.VERIFY.policy()){
        return "Expected verify mode off by default.";
    }
    options.VERIFY.set_enabled(true);
    if (options.check_validity().empty()){
        return "Expected verify mode refused over more than one checkpoint.";
    }
    options.END_CHECKPOINT.set_by_index(options.start_index());
    const std::optional<VerifyPolicy> policy = options.VERIFY.policy();
    if (!options.check_validity().empty() || !policy || policy->runs != 3 || policy->passes_needed != 2){
        return std::format("Expected 3 runs and 2 passes on one checkpoint: {}", options.check_validity());
    }
    options.VERIFY.RUNS.set(2);
    options.VERIFY.PASSES.set(3);
    if (options.check_validity().empty()){
        return "Expected more passes than runs refused.";
    }
    return true;
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


struct StoryStepInput{
    std::string name;
    UnitTestResult (*test)();
};

struct StoryStepExpectation{
    std::string_view description;
};

struct StoryStepTestCase{
    StoryStepInput input;
    StoryStepExpectation expectation;
};

class Test_StoryStepExecutor : public UnitTest{
public:
    explicit Test_StoryStepExecutor(StoryStepTestCase scenario)
        : UnitTest(std::format("Pokemon::StoryStepExecutor - {}", scenario.input.name))
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
        .input = {.name = "steps run in table order", .test = test_events_in_table_order},
        .expectation = {.description = "executes story steps sequentially as defined in table"},
    },
    {
        .input = {.name = "skip_if asks the executor", .test = test_skip_if},
        .expectation = {.description = "delegates skip condition check to executor"},
    },
    {
        .input = {.name = "each step starts where the last left the player", .test = test_pose_propagation},
        .expectation = {.description = "propagates player pose across sequential step executions"},
    },
    {
        .input = {.name = "a failure names its step", .test = test_failure_names_the_step},
        .expectation = {.description = "records failing step identifier on error"},
    },
    {
        .input = {.name = "unsupported kinds and lost battles", .test = test_unsupported_and_lost_battles},
        .expectation = {.description = "flags unsupported step types and handles battle defeat"},
    },
    {
        .input = {.name = "tables as a segment, with a reload", .test = test_segment_of_tables},
        .expectation = {.description = "converts step tables into segment with checkpoint reload support"},
    },
    {
        .input = {.name = "lint", .test = test_lint},
        .expectation = {.description = "verifies lint validation rules on story checkpoints"},
    },
    {
        .input = {.name = "the run options' defaults", .test = test_run_options},
        .expectation = {.description = "validates initial default story run option values"},
    },
    {
        .input = {.name = "the run options' verify mode", .test = test_verify_options},
        .expectation = {.description = "configures verification mode options and passes count"},
    },
    {
        .input = {.name = "start and end checkpoint dropdowns", .test = test_checkpoint_dropdowns},
        .expectation = {.description = "populates start and end dropdown selections correctly"},
    },
    {
        .input = {.name = "a segment's run in the AutoStory stats", .test = test_story_stats},
        .expectation = {.description = "updates story run statistics following segment completion"},
    },
    {
        .input = {.name = "policy: attempts per checkpoint, then fail", .test = test_policy_attempts_then_fail},
        .expectation = {.description = "enforces max checkpoint attempt policy before terminal failure"},
    },
    {
        .input = {.name = "policy: one attempt stops at the first failure", .test = test_policy_stop_at_first_failure},
        .expectation = {.description = "halts without a reload when a checkpoint has one attempt"},
    },
    {
        .input = {.name = "once per save: a retry skips what changed nothing", .test = test_once_per_save_retry},
        .expectation = {.description = "skips steps that changed nothing during once per save retry"},
    },
    {
        .input = {.name = "once per save: verify runs prepare once", .test = test_once_per_save_verify},
        .expectation = {.description = "executes once per save prepare step only once during verification"},
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
