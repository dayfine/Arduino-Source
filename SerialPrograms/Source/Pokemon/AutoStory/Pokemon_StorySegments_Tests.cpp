/*  Story Segments Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The segment runner against a scripted world with a save slot: a few
 *  fields the checkpoints' events change, saved and reloaded by the hooks.
 *
 */

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_StorySegments.h"
#include "Pokemon_TilePose.h"
#include "Pokemon_AutoStory_Tests.h"
#include "Pokemon_AutoStoryFailureCapture_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  Twinleaf Town to Sandgem Town, shortened.
struct World{
    std::string map = "Twinleaf Town";
    bool parcel = false;
    //  Walks south that fail before one gets through.
    int walk_failures = 0;
    std::vector<std::string> trace;
};

Pose at_home_door(){
    Pose pose;
    pose.map = "Twinleaf Town";
    pose.certainty = PoseCertainty::TILE;
    pose.where = std::make_shared<const TilePose>(std::vector<TilePos>{TilePos{5, 9}}, Direction::SOUTH);
    return pose;
}

//  The world, its save slot, and the log.
struct Lab{
    World world;
    World saved;
    std::vector<std::string> lines;

    SegmentHooks hooks(){
        SegmentHooks hooks;
        hooks.save = [this](RunState&){
            world.trace.push_back("save");
            saved = world;
            saved.trace.clear();
            return outcome_ok();
        };
        hooks.reload = [this](RunState&){
            //  Whatever blocked a walk has moved on by the reload.
            const int failures = world.walk_failures;
            std::vector<std::string> trace = std::move(world.trace);
            world = saved;
            world.walk_failures = failures;
            world.trace = std::move(trace);
            world.trace.push_back("reload");
            return outcome_ok(at_home_door());
        };
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        return hooks;
    }
    bool has(const std::string& line) const{
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    }
    std::string all() const{
        std::string text;
        for (const std::string& line : lines){
            text += "\n" + line;
        }
        return text;
    }
};

Event make_event(std::string name, Act act){
    Event event;
    event.name = std::move(name);
    event.act = std::move(act);
    return event;
}

Condition on_map(World& world, std::string map){
    return condition("on " + map, [&world, map](RunState&){ return world.map == map; });
}

CheckpointSpec leave_home(World& world){
    CheckpointSpec spec;
    spec.id = "00.01";
    spec.title = "Leave the house";
    spec.saves = true;
    spec.build = [&world]{
        Checkpoint checkpoint;
        checkpoint.id = "00.01";
        Event walk = make_event("walk_out", [&world](RunState&){
            world.trace.push_back("walk_out");
            world.map = "Route 201";
            return outcome_ok();
        });
        walk.why = "the story starts outside";
        walk.skip_if.push_back(on_map(world, "Route 201"));
        checkpoint.events.push_back(std::move(walk));
        checkpoint.end.push_back(on_map(world, "Route 201"));
        return checkpoint;
    };
    return spec;
}

CheckpointSpec walk_to_sandgem(World& world){
    CheckpointSpec spec;
    spec.id = "00.02";
    spec.title = "Walk to Sandgem Town";
    spec.build = [&world]{
        Checkpoint checkpoint;
        checkpoint.id = "00.02";
        Event walk = make_event("walk_south", [&world](RunState&){
            world.trace.push_back("walk_south");
            if (world.walk_failures > 0){
                world.walk_failures--;
                return outcome_failed("blocked", "a wanderer on the path");
            }
            world.map = "Sandgem Town";
            return outcome_ok();
        });
        walk.require.push_back(on_map(world, "Route 201"));
        walk.expect.push_back(on_map(world, "Sandgem Town"));
        Recovery stop;
        stop.kind = RecoveryKind::STOP;
        stop.why = "the path is one tile wide";
        walk.recoveries.push_back(std::move(stop));
        checkpoint.events.push_back(std::move(walk));
        return checkpoint;
    };
    return spec;
}

CheckpointSpec get_parcel(World& world){
    CheckpointSpec spec;
    spec.id = "00.03";
    spec.title = "Get the Parcel";
    spec.build = [&world]{
        Checkpoint checkpoint;
        checkpoint.id = "00.03";
        Event talk = make_event("talk_clerk", [&world](RunState&){
            world.trace.push_back("talk_clerk");
            world.parcel = true;
            return outcome_ok();
        });
        talk.expect.push_back(condition("parcel in the Bag", [&world](RunState&){ return world.parcel; }));
        talk.pose_learned = at_home_door();
        checkpoint.events.push_back(std::move(talk));
        return checkpoint;
    };
    return spec;
}

Segment twinleaf_to_sandgem(World& world){
    Segment segment;
    segment.id = "00";
    segment.checkpoints.push_back(leave_home(world));
    segment.checkpoints.push_back(walk_to_sandgem(world));
    segment.checkpoints.push_back(get_parcel(world));
    return segment;
}


UnitTestResult test_runs_checkpoints_in_order(){
    Lab lab;
    Segment segment = twinleaf_to_sandgem(lab.world);
    SegmentHooks hooks = lab.hooks();
    RunState state;
    const SegmentReport report = run_segment(state, segment, SegmentRange{}, SegmentPolicy{}, hooks);
    const std::vector<std::string> expected = {"walk_out", "save", "walk_south", "talk_clerk"};
    if (!report.completed || report.checkpoints_run != 3 || lab.world.trace != expected){
        return "Expected three checkpoints, saved after the first:" + lab.all();
    }
    if (!lab.has("[Segment] 00: start, 3 checkpoints") || lab.lines.back() != "[Segment] 00: done, 3 checkpoints"){
        return "Unexpected log:" + lab.all();
    }
    if (lab.saved.map != "Route 201"){
        return "The save should hold the world after the first checkpoint.";
    }
    return true;
}

UnitTestResult test_reloads_a_failed_checkpoint(){
    Lab lab;
    lab.world.walk_failures = 1;
    Segment segment = twinleaf_to_sandgem(lab.world);
    SegmentHooks hooks = lab.hooks();
    RunState state;
    const SegmentReport report = run_segment(state, segment, SegmentRange{}, SegmentPolicy{}, hooks);
    const std::vector<std::string> expected = {"walk_out", "save", "walk_south", "reload", "walk_south", "talk_clerk"};
    if (!report.completed || report.reloads != 1 || lab.world.trace != expected){
        return "Expected a reload, then the walk again:" + lab.all();
    }
    if (!lab.has("[Segment] 00: 00.02 failed, reloading for attempt 2/3")){
        return "Expected the reload logged:" + lab.all();
    }
    return true;
}

UnitTestResult test_gives_up_and_names_the_failure(){
    Lab lab;
    lab.world.walk_failures = 10;
    Segment segment = twinleaf_to_sandgem(lab.world);
    SegmentHooks hooks = lab.hooks();
    RunState state;
    const SegmentReport report = run_segment(state, segment, SegmentRange{}, SegmentPolicy{.attempts_per_checkpoint = 2}, hooks);
    if (report.completed || report.failed_checkpoint != "00.02" || report.failed_event != "walk_south"){
        return "Expected the failed checkpoint and event named:" + lab.all();
    }
    if (report.failure.failure != "blocked" || report.reloads != 1 || report.checkpoints_run != 2){
        return "Expected two attempts with one reload between them:" + lab.all();
    }
    if (lab.world.parcel || lab.lines.back() != "[Segment] 00: failed at 00.02 walk_south: blocked: a wanderer on the path"){
        return "Nothing after the failed checkpoint should run:" + lab.all();
    }
    if (state.pose.map != "Twinleaf Town" || state.pose.certainty != PoseCertainty::TILE){
        return "The reload should have set the pose: " + to_string(state.pose);
    }
    return true;
}

UnitTestResult test_range(){
    Lab lab;
    lab.world.map = "Route 201";
    Segment segment = twinleaf_to_sandgem(lab.world);
    SegmentHooks hooks = lab.hooks();
    RunState state;
    SegmentReport report = run_segment(state, segment, SegmentRange{.from = "00.02", .to = "00.02"}, SegmentPolicy{}, hooks);
    if (!report.completed || lab.world.trace != std::vector<std::string>{"walk_south"}){
        return "Expected only the second checkpoint:" + lab.all();
    }
    report = run_segment(state, segment, SegmentRange{.from = "00.09", .to = std::nullopt}, SegmentPolicy{}, hooks);
    if (report.completed || report.failure.failure != "unknown_checkpoint" || report.failure.detail != "00.09"){
        return "Expected the unknown id reported:" + lab.all();
    }
    report = run_segment(state, segment, SegmentRange{.from = "00.03", .to = "00.01"}, SegmentPolicy{}, hooks);
    if (report.completed || report.failure.failure != "bad_range"){
        return "Expected the reversed range reported:" + lab.all();
    }
    return true;
}

//  The status sink gets every log line again, without its tag, for the
//  video overlay.
UnitTestResult test_status_lines(){
    Lab lab;
    Segment segment = twinleaf_to_sandgem(lab.world);
    SegmentHooks hooks = lab.hooks();
    std::vector<std::string> status;
    hooks.status = [&status](const std::string& line){ status.push_back(line); };
    RunState state;
    run_segment(state, segment, SegmentRange{}, SegmentPolicy{}, hooks);
    if (status.size() != lab.lines.size() || status.empty()){
        return "Expected a status line per log line:" + lab.all();
    }
    for (size_t c = 0; c < status.size(); c++){
        const std::string& line = lab.lines[c];
        if (line.size() <= status[c].size() || line.compare(line.size() - status[c].size(), status[c].size(), status[c]) != 0){
            return "Status \"" + status[c] + "\" isn't log line \"" + line + "\" without its tag.";
        }
    }
    if (std::find(status.begin(), status.end(), "00.01/1 walk_out: start") == status.end()){
        return "Expected \"00.01/1 walk_out: start\" on the overlay.";
    }
    return true;
}

//  An event that throws (a console step failing deep down) is logged as
//  failed, with its checkpoint, and the exception goes on to the game.
UnitTestResult test_throwing_event_is_logged(){
    Lab lab;
    Segment segment;
    segment.id = "00";
    CheckpointSpec spec;
    spec.id = "00.09";
    spec.build = []{
        Checkpoint checkpoint;
        checkpoint.id = "00.09";
        checkpoint.events.push_back(make_event("brock", [](RunState&) -> Outcome{
            throw std::runtime_error("the console vanished");
        }));
        return checkpoint;
    };
    segment.checkpoints.push_back(std::move(spec));
    SegmentHooks hooks = lab.hooks();
    RunState state;
    try{
        run_segment(state, segment, SegmentRange{}, SegmentPolicy{}, hooks);
    }catch (std::runtime_error&){
        if (!lab.has("[Event] 00.09/1 brock: failed: the console vanished")
            || !lab.has("[Checkpoint] 00.09: failed at 1 brock: the console vanished")
        ){
            return "Expected the event and checkpoint logged as failed:" + lab.all();
        }
        return true;
    }
    return "Expected the exception to go on to the game.";
}

//  Verify mode: every run starts from the save the first started from, a
//  failed run is counted, not retried, and 2 of 3 passes verify it.
UnitTestResult test_verify_runs_from_one_save(){
    Lab lab;
    lab.world.map = "Route 201";
    lab.saved = lab.world;
    lab.world.walk_failures = 1;
    Segment segment = twinleaf_to_sandgem(lab.world);
    SegmentHooks hooks = lab.hooks();
    RunState state;
    const VerifyReport report = verify_checkpoint(state, segment, "00.02", VerifyPolicy{}, hooks);
    const std::vector<std::string> expected = {"walk_south", "reload", "walk_south", "reload", "walk_south"};
    if (!report.verified || report.runs != 3 || report.passes != 2 || lab.world.trace != expected){
        return "Expected three runs, reloading between them, two passing:" + lab.all();
    }
    if (!lab.has("[Verify] 00.02: start, 3 runs from one save, 2 must pass")
        || !lab.has("[Verify] 00.02: run 1/3 failed at walk_south: blocked: a wanderer on the path")
        || !lab.has("[Verify] 00.02: run 2/3 passed")
        || lab.lines.back() != "[Verify] 00.02: 2/3 passed, 2 needed: verified"
    ){
        return "Expected a line per run, then the summary:" + lab.all();
    }
    return true;
}

//  A checkpoint that saves at its end doesn't in verify mode: the next run
//  would start after it.
UnitTestResult test_verify_skips_the_end_save(){
    Lab lab;
    Segment segment = twinleaf_to_sandgem(lab.world);
    SegmentHooks hooks = lab.hooks();
    RunState state;
    const VerifyReport report = verify_checkpoint(state, segment, "00.01", VerifyPolicy{.runs = 2, .passes_needed = 2}, hooks);
    const std::vector<std::string> expected = {"walk_out", "reload", "walk_out"};
    if (!report.verified || lab.world.trace != expected || lab.saved.map != "Twinleaf Town"){
        return "Expected two runs from Twinleaf Town and no save:" + lab.all();
    }
    return true;
}

UnitTestResult test_verify_too_few_passes(){
    Lab lab;
    lab.world.map = "Route 201";
    lab.saved = lab.world;
    lab.world.walk_failures = 2;
    Segment segment = twinleaf_to_sandgem(lab.world);
    SegmentHooks hooks = lab.hooks();
    RunState state;
    const VerifyReport report = verify_checkpoint(state, segment, "00.02", VerifyPolicy{}, hooks);
    if (report.verified || report.runs != 3 || report.passes != 1
        || lab.lines.back() != "[Verify] 00.02: 1/3 passed, 2 needed: not verified"
    ){
        return "Expected 1 of 3, not verified:" + lab.all();
    }
    VerifyReport bad = verify_checkpoint(state, segment, "00.02", VerifyPolicy{.runs = 2, .passes_needed = 3}, hooks);
    if (bad.verified || bad.runs != 0 || bad.failure.failure != "bad_verify_policy"){
        return "Expected more passes than runs refused:" + lab.all();
    }
    bad = verify_checkpoint(state, segment, "00.09", VerifyPolicy{}, hooks);
    if (bad.verified || bad.runs != 0 || bad.failure.failure != "unknown_checkpoint"){
        return "Expected an unknown checkpoint refused:" + lab.all();
    }
    return true;
}

UnitTestResult test_describe(){
    Lab lab;
    Segment segment = twinleaf_to_sandgem(lab.world);
    const std::string expected =
        "segment 00: 3 checkpoints\n"
        "  00.01 \"Leave the house\", saves\n"
        "  00.02 \"Walk to Sandgem Town\"\n"
        "  00.03 \"Get the Parcel\"\n"
        "checkpoint 00.01: 1 events\n"
        "  event 1 walk_out: the story starts outside\n"
        "    skip_if: on Route 201\n"
        "  end: on Route 201\n"
        "checkpoint 00.02: 1 events\n"
        "  event 1 walk_south\n"
        "    require: on Route 201\n"
        "    expect: on Sandgem Town\n"
        "    recovery: stop on any failure: the path is one tile wide\n"
        "checkpoint 00.03: 1 events\n"
        "  event 1 talk_clerk\n"
        "    expect: parcel in the Bag\n"
        "    pose_learned: Twinleaf Town (5, 9) facing south (tile)\n";
    const std::string text = describe(segment);
    if (text != expected){
        return "Unexpected description:\n" + text;
    }
    if (!lab.world.trace.empty()){
        return "Describing a segment should run nothing.";
    }
    return true;
}

//  The failed hook hears of each failed attempt and verify run, before the
//  reload that would take the failure off the screen.
UnitTestResult test_failed_hook_before_reload(){
    Lab lab;
    lab.world.walk_failures = 10;
    Segment segment = twinleaf_to_sandgem(lab.world);
    SegmentHooks hooks = lab.hooks();
    hooks.failed = [&lab](const std::string& checkpoint, const std::string& event, const Outcome& failure){
        lab.world.trace.push_back("failed " + checkpoint + " " + event + " " + to_string(failure));
    };
    RunState state;
    run_segment(state, segment, SegmentRange{}, SegmentPolicy{.attempts_per_checkpoint = 2}, hooks);
    const std::string failed = "failed 00.02 walk_south blocked: a wanderer on the path";
    const std::vector<std::string> expected = {"walk_out", "save", "walk_south", failed, "reload", "walk_south", failed};
    if (lab.world.trace != expected){
        return "Expected each failed attempt told before its reload:" + lab.all();
    }
    lab.world.trace.clear();
    lab.world.walk_failures = 1;
    lab.saved = lab.world;
    lab.saved.map = "Route 201";
    lab.world.map = "Route 201";
    verify_checkpoint(state, segment, "00.02", VerifyPolicy{}, hooks);
    if (lab.world.trace.size() < 2 || lab.world.trace[1] != failed){
        return "Expected the failed verify run told:" + lab.all();
    }
    return true;
}


struct StorySegmentInput{
    std::string name;
    UnitTestResult (*test)();
};

struct StorySegmentExpectation{
    std::string_view description;
};

struct StorySegmentTestCase{
    StorySegmentInput input;
    StorySegmentExpectation expectation;
};

class Test_StorySegments : public UnitTest{
public:
    explicit Test_StorySegments(StorySegmentTestCase scenario)
        : UnitTest("Pokemon::StorySegments - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.test();
    }

private:
    StorySegmentTestCase m_scenario;
};

const StorySegmentTestCase STORY_SEGMENT_CASES[] = {
    {
        .input = {.name = "failed hook before reload", .test = test_failed_hook_before_reload},
        .expectation = {.description = "tells the game of each failed attempt while the failure is on screen"},
    },
    {
        .input = {.name = "checkpoints run in order", .test = test_runs_checkpoints_in_order},
        .expectation = {.description = "executes sequential checkpoints and saves state"},
    },
    {
        .input = {.name = "a failed checkpoint is reloaded", .test = test_reloads_a_failed_checkpoint},
        .expectation = {.description = "reloads prior saved state upon checkpoint failure"},
    },
    {
        .input = {.name = "gives up and names the failure", .test = test_gives_up_and_names_the_failure},
        .expectation = {.description = "reports terminal checkpoint failure after exhausting retries"},
    },
    {
        .input = {.name = "a range of checkpoints", .test = test_range},
        .expectation = {.description = "runs only the specified slice of checkpoints"},
    },
    {
        .input = {.name = "describe", .test = test_describe},
        .expectation = {.description = "formats human-readable segment summary without executing"},
    },
    {
        .input = {.name = "status lines for the overlay", .test = test_status_lines},
        .expectation = {.description = "generates UI overlay status updates during execution"},
    },
    {
        .input = {.name = "a throwing event is logged, then goes on", .test = test_throwing_event_is_logged},
        .expectation = {.description = "handles and logs exception without terminating segment"},
    },
    {
        .input = {.name = "verify: runs from one save", .test = test_verify_runs_from_one_save},
        .expectation = {.description = "runs verification passes against the initial save state"},
    },
    {
        .input = {.name = "verify: no save at the end", .test = test_verify_skips_the_end_save},
        .expectation = {.description = "skips checkpoint state save during verification mode"},
    },
    {
        .input = {.name = "verify: too few passes", .test = test_verify_too_few_passes},
        .expectation = {.description = "fails verification when required pass count is not met"},
    },
};

} // namespace


void add_tests_StorySegments(UnitTestDatabase& database){
    for (const auto& scenario : STORY_SEGMENT_CASES){
        database.add<Test_StorySegments>(scenario);
    }
    //  The failure capture is what the games attach to the segment's
    //  failed hook; its tests run with the segment's.
    add_tests_AutoStoryFailureCapture(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
