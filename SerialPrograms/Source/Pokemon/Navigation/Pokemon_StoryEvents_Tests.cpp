/*  Story Events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The checkpoint runner against a scripted world: a few fields that the
 *  events' actions change and their checks read, instead of a console.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_TilePose.h"
#include "Pokemon_Navigation_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  A small slice of Route 201/202 and the player's house.
struct World{
    std::string map = "Route 202";
    int potions = 0;
    bool parcel = false;
    bool dialogue_open = false;
    bool battle_showing = false;
    //  Talks that open nothing before the NPC answers (she is wandering).
    int talks_before_answer = 0;
    //  Rows between us and Mom; a talk from a row off reaches nobody.
    int rows_off = 0;
    //  Walks that fail before one gets through.
    int walk_failures = 0;
    std::vector<std::string> trace;
};

struct Lines{
    std::vector<std::string> lines;

    EventLogger logger(){
        return [this](const std::string& line){ lines.push_back(line); };
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

Act record(World& world, std::string what){
    return [&world, what = std::move(what)](RunState&){
        world.trace.push_back(what);
        return outcome_ok();
    };
}

Condition has_potions(World& world){
    return condition("potions in the Bag", [&world](RunState&){ return world.potions > 0; });
}

Condition dialogue_opened(World& world){
    return condition("dialogue opened", [&world](RunState&){ return world.dialogue_open; });
}

//  The Poke Mart lady: answers once she has wandered back, gives 10 Potions.
Event talk_to_lady(World& world){
    Event event = make_event("talk_poke_mart_lady", [&world](RunState&){
        world.trace.push_back("talk");
        if (world.talks_before_answer > 0){
            world.talks_before_answer--;
            return outcome_ok();
        }
        world.dialogue_open = true;
        world.potions += 10;
        return outcome_ok();
    });
    event.skip_if.push_back(has_potions(world));
    event.expect.push_back(dialogue_opened(world));
    return event;
}

Recovery recovery(std::string on, RecoveryKind kind, size_t attempts, Act prepare = nullptr){
    Recovery recovery;
    recovery.on = std::move(on);
    recovery.kind = kind;
    recovery.attempts = attempts;
    recovery.prepare = std::move(prepare);
    return recovery;
}

Checkpoint make_checkpoint(){
    Checkpoint checkpoint;
    checkpoint.id = "00.04";
    return checkpoint;
}


UnitTestResult test_runs_events_in_order(){
    World world;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.events.push_back(make_event("walk_to_corner", record(world, "walk")));
    checkpoint.events.push_back(make_event("talk_lady", record(world, "talk")));
    checkpoint.events.push_back(make_event("save", record(world, "save")));
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    const std::vector<std::string> expected = {
        "[Checkpoint] 00.04: start, 3 events",
        "[Event] 00.04/1 walk_to_corner: start",
        "[Event] 00.04/1 walk_to_corner: done",
        "[Event] 00.04/2 talk_lady: start",
        "[Event] 00.04/2 talk_lady: done",
        "[Event] 00.04/3 save: start",
        "[Event] 00.04/3 save: done",
        "[Checkpoint] 00.04: done, 3 run, 0 skipped",
    };
    if (!report.completed || lines.lines != expected){
        return "Unexpected log:" + lines.all();
    }
    if (world.trace != std::vector<std::string>{"walk", "talk", "save"}){
        return "Events ran out of order.";
    }
    return true;
}

UnitTestResult test_skips_done_event(){
    World world;
    world.potions = 10;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.events.push_back(talk_to_lady(world));
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    if (!report.completed || report.events_skipped != 1 || report.events_run != 0){
        return "Expected the event skipped.";
    }
    if (!world.trace.empty() || !lines.has("[Event] 00.04/1 talk_poke_mart_lady: skipped")){
        return "The skipped event should not act:" + lines.all();
    }
    return true;
}

UnitTestResult test_expect_failure_waited_out(){
    World world;
    world.talks_before_answer = 2;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.events.push_back(talk_to_lady(world));
    checkpoint.events.back().recoveries.push_back(recovery("expect_failed", RecoveryKind::WAIT, 2));
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    if (!report.completed || world.trace.size() != 3 || world.potions != 10){
        return "Expected three talks, the last answered:" + lines.all();
    }
    if (!lines.has("[Event] 00.04/1 talk_poke_mart_lady: recover: wait (expect_failed)")){
        return "Expected the wait logged:" + lines.all();
    }
    return true;
}

//  Mom: a talk from the wrong row reaches nobody. Waiting does not help;
//  confirming the row on a landmark does.
Event talk_to_mom(World& world){
    Event event = make_event("talk_mom", [&world](RunState&){
        world.trace.push_back("talk");
        world.dialogue_open = world.rows_off == 0;
        return outcome_ok();
    });
    event.expect.push_back(dialogue_opened(world));
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::WAIT, 1, record(world, "wait")));
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::REANCHOR, 1, [&world](RunState& state){
        world.trace.push_back("anchor");
        world.rows_off = 0;
        state.pose.certainty = PoseCertainty::TILE;
        return outcome_ok();
    }));
    event.recoveries.push_back(recovery("", RecoveryKind::STOP, 0));
    event.recoveries.back().why = "four rows cover the mat to the TV";
    return event;
}

UnitTestResult test_escalates_to_reanchor(){
    World world;
    world.rows_off = 1;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.events.push_back(talk_to_mom(world));
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    const std::vector<std::string> expected = {"talk", "wait", "talk", "anchor", "talk"};
    if (!report.completed || world.trace != expected){
        return "Expected wait, then re-anchor:" + lines.all();
    }
    if (state.pose.certainty != PoseCertainty::TILE){
        return "The re-anchor should have fixed the pose.";
    }
    return true;
}

UnitTestResult test_stop_names_failed_event(){
    World world;
    world.rows_off = 1;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.events.push_back(make_event("enter_home", record(world, "enter")));
    checkpoint.events.push_back(talk_to_mom(world));
    checkpoint.events.back().recoveries[1].prepare = record(world, "anchor");     //  An anchor that does not help.
    checkpoint.events.push_back(make_event("mom_dialogue", record(world, "dialogue")));
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    if (report.completed || report.failed_event != 2 || report.failed_name != "talk_mom"){
        return "Expected event 2 reported:" + lines.all();
    }
    if (report.failure.failure != "expect_failed" || report.failure.detail != "dialogue opened"){
        return "Expected the failing expect reported.";
    }
    if (std::count(world.trace.begin(), world.trace.end(), "dialogue") != 0){
        return "Nothing after the failed event should run.";
    }
    if (!lines.has("[Event] 00.04/2 talk_mom: recover: stop (expect_failed): four rows cover the mat to the TV")
        || lines.lines.back() != "[Checkpoint] 00.04: failed at 2 talk_mom: expect_failed: dialogue opened"
    ){
        return "Unexpected log:" + lines.all();
    }
    return true;
}

UnitTestResult test_unmatched_failure_stops(){
    World world;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.events.push_back(make_event("probe_for_mom", [&world](RunState&){
        world.trace.push_back("probe");
        return outcome_failed("probe_exhausted", "4 rows");
    }));
    checkpoint.events.back().recoveries.push_back(recovery("blocked", RecoveryKind::WAIT, 3));
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    if (report.completed || report.failure.failure != "probe_exhausted" || world.trace.size() != 1){
        return "A failure no recovery names should stop at once:" + lines.all();
    }
    if (!lines.has("[Event] 00.04/1 probe_for_mom: failed: probe_exhausted: 4 rows")){
        return "Unexpected log:" + lines.all();
    }
    return true;
}

Interrupt wild_battle(World& world, ResumePolicy resume){
    Interrupt interrupt;
    interrupt.name = "wild_battle";
    interrupt.detect = [&world](RunState&){ return world.battle_showing; };
    interrupt.handle = [&world](RunState&){
        world.trace.push_back("flee");
        world.battle_showing = false;
        return outcome_ok();
    };
    interrupt.pose = InterruptPose::WITHIN_SET;
    interrupt.resume = resume;
    return interrupt;
}

//  A walk through grass: the first try meets a wild battle half-way.
Event walk_through_grass(World& world){
    Event event = make_event("walk_to_corner", [&world](RunState&){
        world.trace.push_back("walk");
        if (world.walk_failures > 0){
            world.walk_failures--;
            world.battle_showing = true;
            return outcome_failed("walk_stopped");
        }
        world.map = "Route 201";
        return outcome_ok();
    });
    event.require.push_back(pose_known("Route 202", PoseCertainty::TILE));
    event.expect.push_back(condition("on Route 201", [&world](RunState&){ return world.map == "Route 201"; }));
    Pose corner;
    corner.map = "Route 201";
    corner.certainty = PoseCertainty::TILE;
    corner.where = std::make_shared<const TilePose>(std::vector<TilePos>{TilePos{42, 3}});
    event.pose_learned = corner;
    event.recoveries.push_back(recovery("require_failed", RecoveryKind::REANCHOR, 1, [&world](RunState& state){
        world.trace.push_back("anchor");
        state.pose.certainty = PoseCertainty::TILE;
        return outcome_ok();
    }));
    return event;
}

RunState at_catch_spot(std::optional<Direction> facing = std::nullopt){
    RunState state;
    state.pose.map = "Route 202";
    state.pose.certainty = PoseCertainty::TILE;
    state.pose.where = std::make_shared<const TilePose>(std::vector<TilePos>{TilePos{10, 20}}, facing);
    return state;
}

UnitTestResult test_interrupt_mid_event(){
    World world;
    world.walk_failures = 1;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.interrupts.push_back(wild_battle(world, ResumePolicy::RETRY_EVENT));
    checkpoint.events.push_back(walk_through_grass(world));
    RunState state = at_catch_spot();
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    //  The battle leaves the pose inexact, so the retry first re-anchors.
    const std::vector<std::string> expected = {"walk", "flee", "anchor", "walk"};
    if (!report.completed || world.trace != expected){
        return "Expected flee, re-anchor, walk again:" + lines.all();
    }
    if (!lines.has("[Event] 00.04/1 walk_to_corner: interrupt: wild_battle")){
        return "Expected the interrupt logged:" + lines.all();
    }
    if (state.pose.map != "Route 201" || tiles_of(state.pose) != std::vector<TilePos>{TilePos{42, 3}}){
        return "Expected the learned pose: " + to_string(state.pose);
    }
    return true;
}

UnitTestResult test_interrupt_recheck_expect(){
    World world;
    Checkpoint checkpoint = make_checkpoint();
    Interrupt interrupt = wild_battle(world, ResumePolicy::RECHECK_EXPECT);
    interrupt.pose = InterruptPose::UNCHANGED;
    checkpoint.interrupts.push_back(std::move(interrupt));
    //  A talk-only trainer stops us on the last step: we arrived anyway.
    checkpoint.events.push_back(make_event("walk_to_exit", [&world](RunState&){
        world.trace.push_back("walk");
        world.map = "Route 201";
        world.battle_showing = true;
        return outcome_failed("walk_stopped");
    }));
    checkpoint.events.back().expect.push_back(condition("on Route 201", [&world](RunState&){ return world.map == "Route 201"; }));
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    if (!report.completed || world.trace != std::vector<std::string>{"walk", "flee"}){
        return "Expected done without walking again:" + lines.all();
    }
    return true;
}

UnitTestResult test_too_many_interrupts(){
    World world;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.max_interrupts_per_event = 2;
    checkpoint.interrupts.push_back(wild_battle(world, ResumePolicy::RETRY_EVENT));
    checkpoint.events.push_back(make_event("walk", [&world](RunState&){
        world.battle_showing = true;
        return outcome_failed("walk_stopped");
    }));
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    if (report.completed || report.failure.failure != "too_many_interrupts"){
        return "Expected to give up after 2 interrupts:" + lines.all();
    }
    return true;
}


//  00.04 after the catch, shortened: Potions, save, then a walk north that
//  fails on the first run. The reload restores the world as saved.
Checkpoint post_catch(World& world, World& saved){
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.events.push_back(talk_to_lady(world));
    Event save = make_event("save_on_202", [&world, &saved](RunState&){
        world.trace.push_back("save");
        world.map = "Route 202";
        saved = world;
        return outcome_ok();
    });
    save.skip_if.push_back(has_potions(world));
    save.skip_if.push_back(condition("on Route 202", [&world](RunState&){ return world.map == "Route 202"; }));
    checkpoint.events.push_back(std::move(save));
    checkpoint.events.push_back(make_event("train", record(world, "train")));
    checkpoint.events.push_back(make_event("walk_north", [&world](RunState&){
        world.trace.push_back("walk");
        if (world.walk_failures > 0){
            world.walk_failures--;
            return outcome_failed("blocked_unexplained");
        }
        world.map = "Jubilife City";
        return outcome_ok();
    }));
    checkpoint.end.push_back(condition("in Jubilife", [&world](RunState&){ return world.map == "Jubilife City"; }));
    return checkpoint;
}

UnitTestResult test_resume_after_reload(){
    World world;
    world.map = "Route 201";
    world.walk_failures = 1;
    World saved;
    Checkpoint checkpoint = post_catch(world, saved);
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport first = run_checkpoint(state, checkpoint, log);
    if (first.completed || first.failed_event != 4){
        return "Expected the first run to fail at the walk:" + lines.all();
    }
    world = saved;      //  Reload: the save has the Potions and stands on Route 202.
    world.walk_failures = 0;    //  Whatever blocked the walk has moved on.
    world.trace.clear();
    RunState reloaded;
    const CheckpointReport second = run_checkpoint(reloaded, checkpoint, log);
    if (!second.completed || second.events_skipped != 2 || second.events_run != 2){
        return "Expected the Potions and the save skipped:" + lines.all();
    }
    if (world.trace != std::vector<std::string>{"train", "walk"} || world.potions != 10){
        return "Expected only the training and the walk again.";
    }
    return true;
}

UnitTestResult test_start_end_and_save(){
    World world;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.start.push_back(has_potions(world));
    checkpoint.events.push_back(make_event("walk", record(world, "walk")));
    checkpoint.end.push_back(condition("parcel in the Bag", [&world](RunState&){ return world.parcel; }));
    checkpoint.save = record(world, "save");
    RunState state;
    Lines lines;
    EventLogger log = lines.logger();
    CheckpointReport report = run_checkpoint(state, checkpoint, log);
    if (report.completed || report.failure.failure != "start_failed" || !world.trace.empty()){
        return "Expected the start check to stop before any event.";
    }
    world.potions = 1;
    report = run_checkpoint(state, checkpoint, log);
    if (report.completed || report.failure.failure != "end_failed" || world.trace.size() != 1){
        return "Expected the end check to fail without a save.";
    }
    world.parcel = true;
    report = run_checkpoint(state, checkpoint, log);
    if (!report.completed || world.trace.back() != "save"){
        return "Expected a save after the end check.";
    }
    return true;
}

UnitTestResult test_pose_certainty(){
    if (!certainty_covers(PoseCertainty::TILE, PoseCertainty::COLUMN)
        || certainty_covers(PoseCertainty::ROW, PoseCertainty::COLUMN)
        || certainty_covers(PoseCertainty::SET, PoseCertainty::ROW)
        || !certainty_covers(PoseCertainty::SET, PoseCertainty::SET)
        || certainty_covers(PoseCertainty::UNKNOWN, PoseCertainty::SET)
        || !certainty_covers(PoseCertainty::UNKNOWN, PoseCertainty::UNKNOWN)
    ){
        return "Wrong certainty order.";
    }
    RunState state = at_catch_spot(Direction::NORTH);
    if (to_string(state.pose) != "Route 202 (10, 20) facing north (tile)"){
        return "Unexpected pose text: " + to_string(state.pose);
    }
    Condition known = pose_known("Route 201", PoseCertainty::SET);
    if (known.check(state)){
        return "The pose is on another map.";
    }
    return true;
}

//  A game without tiles: a point on its map, as Scarlet/Violet would give.
struct PointPose : public Position{
    int x = 0;
    int y = 0;

    PointPose(int x, int y) : x(x), y(y){}
    virtual std::string describe() const override{
        return std::format("({}, {})", x, y);
    }
};

Pose at_point(std::string map, int x, int y){
    Pose pose;
    pose.map = std::move(map);
    pose.certainty = PoseCertainty::TILE;
    pose.where = std::make_shared<const PointPose>(x, y);
    return pose;
}

//  The engine carries whatever position the game gives it: a battle leaves
//  it within the set, a lost pose drops it, a learned pose replaces it.
UnitTestResult test_position_is_the_games(){
    World world;
    world.walk_failures = 1;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.interrupts.push_back(wild_battle(world, ResumePolicy::RETRY_EVENT));
    Event ride = make_event("ride_to_mesagoza", [&world](RunState&){
        world.trace.push_back("ride");
        if (world.walk_failures > 0){
            world.walk_failures--;
            world.battle_showing = true;
            return outcome_failed("walk_stopped");
        }
        return outcome_ok();
    });
    ride.require.push_back(pose_known("South Province", PoseCertainty::SET));
    ride.pose_learned = at_point("Mesagoza", 7, 8);
    checkpoint.events.push_back(std::move(ride));
    RunState state;
    state.pose = at_point("South Province", 3, 4);
    Lines lines;
    EventLogger log = lines.logger();
    const CheckpointReport report = run_checkpoint(state, checkpoint, log);
    if (!report.completed || world.trace != std::vector<std::string>{"ride", "flee", "ride"}){
        return "Expected the ride retried after the battle:" + lines.all();
    }
    if (to_string(state.pose) != "Mesagoza (7, 8) (tile)"){
        return "Expected the learned point: " + to_string(state.pose);
    }
    Interrupt lost = wild_battle(world, ResumePolicy::RETRY_EVENT);
    lost.pose = InterruptPose::LOST;
    checkpoint.interrupts[0] = std::move(lost);
    world.walk_failures = 1;
    checkpoint.events[0].pose_learned.reset();
    checkpoint.events[0].require.clear();
    run_checkpoint(state, checkpoint, log);
    if (state.pose.where || to_string(state.pose) != "Mesagoza (unknown)"){
        return "Expected the lost pose to drop its position: " + to_string(state.pose);
    }
    return true;
}

UnitTestResult test_lint(){
    World world;
    Checkpoint checkpoint = make_checkpoint();
    checkpoint.events.push_back(talk_to_mom(world));
    if (!lint_checkpoint(checkpoint).empty()){
        return "A good checkpoint should lint clean.";
    }
    checkpoint.events.push_back(make_event("talk_mom", nullptr));
    checkpoint.events.back().recoveries.push_back(recovery("", RecoveryKind::STOP, 0));
    checkpoint.events.back().recoveries.push_back(recovery("", RecoveryKind::WAIT, 0));
    const std::vector<std::string> problems = lint_checkpoint(checkpoint);
    //  Duplicate name, no action, wait after stop, wait with no attempts.
    if (problems.size() != 4){
        std::string text;
        for (const std::string& problem : problems){
            text += "\n" + problem;
        }
        return "Expected 4 problems:" + text;
    }
    return true;
}


class Test_StoryEvents : public UnitTest{
public:
    Test_StoryEvents(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("Pokemon::StoryEvents - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

}


void add_tests_StoryEvents(UnitTestDatabase& database){
    database.add<Test_StoryEvents>("events run in order", test_runs_events_in_order);
    database.add<Test_StoryEvents>("a done event is skipped", test_skips_done_event);
    database.add<Test_StoryEvents>("a failed expect is waited out", test_expect_failure_waited_out);
    database.add<Test_StoryEvents>("recoveries escalate to a re-anchor", test_escalates_to_reanchor);
    database.add<Test_StoryEvents>("a stop names the failed event", test_stop_names_failed_event);
    database.add<Test_StoryEvents>("an unanswered failure stops", test_unmatched_failure_stops);
    database.add<Test_StoryEvents>("an interrupt mid-event", test_interrupt_mid_event);
    database.add<Test_StoryEvents>("an interrupt rechecks the expects", test_interrupt_recheck_expect);
    database.add<Test_StoryEvents>("too many interrupts", test_too_many_interrupts);
    database.add<Test_StoryEvents>("resume after a reload", test_resume_after_reload);
    database.add<Test_StoryEvents>("start, end and save", test_start_end_and_save);
    database.add<Test_StoryEvents>("pose certainty", test_pose_certainty);
    database.add<Test_StoryEvents>("the position is the game's", test_position_is_the_games);
    database.add<Test_StoryEvents>("lint", test_lint);
}



}
}
