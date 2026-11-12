/*  Leg Walker Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The leg walker's bookkeeping, with holds that report scripted
 *  interruptions instead of a console.
 *
 */

#include <deque>
#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "Pokemon_LegWalker.h"
#include "Pokemon_LegWalker_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

using namespace std::chrono_literals;

constexpr Milliseconds TILE = 267ms;
constexpr Milliseconds DELAY = 300ms;

//  Holds that report the scripted outcomes in order, then run out.
struct ScriptedLeg{
    std::deque<std::optional<HoldOutcome>> outcomes;
    std::vector<Milliseconds> holds;
    std::vector<size_t> cleared;

    LegCalls calls(){
        LegCalls calls;
        calls.hold_watching = [this](Milliseconds hold) -> std::optional<HoldOutcome>{
            holds.push_back(hold);
            if (outcomes.empty()){
                return std::nullopt;
            }
            std::optional<HoldOutcome> outcome = outcomes.front();
            outcomes.pop_front();
            return outcome;
        };
        calls.clear = [this](size_t index){ cleared.push_back(index); };
        calls.detection_delay = [](size_t){ return DELAY; };
        calls.name = [](size_t){ return std::string("a battle"); };
        calls.log = [](const std::string&){};
        calls.fail = [](const std::string& message){ throw std::runtime_error(message); };
        return calls;
    }
};

//  An interrupt set off by step `step`, seen DELAY after it ends.
HoldOutcome after_step(int step){
    return HoldOutcome{0, TILE * step + DELAY};
}


UnitTestResult test_hold_times(){
    const LegSpec open{Direction::EAST, 4, false, TILE};
    const LegSpec wall{Direction::EAST, 4, true, TILE};
    if (leg_hold(open, 4) != TILE * 3 + TILE / 2){
        return std::format("Open leg: {}ms", leg_hold(open, 4).count());
    }
    if (leg_hold(wall, 4) != TILE * 6){
        return std::format("Wall leg: {}ms", leg_hold(wall, 4).count());
    }
    //  A stall on step 2: a tile longer while the hold covers it.
    LegSpec stalled = open;
    stalled.stalled_steps = {2};
    if (leg_hold(stalled, 4) != TILE * 4 + TILE / 2 || leg_hold(stalled, 3) != TILE * 3 + TILE / 2){
        return std::format("Stalled leg: {}ms, {}ms", leg_hold(stalled, 4).count(), leg_hold(stalled, 3).count());
    }
    if (leg_hold(stalled, 2) != TILE + TILE / 2){
        return std::format("Stalled leg past its stall: {}ms", leg_hold(stalled, 2).count());
    }
    return true;
}

UnitTestResult test_tiles_before_interrupt(){
    const LegSpec leg{Direction::EAST, 8, false, TILE};
    for (int step = 0; step <= 8; step++){
        for (Milliseconds jitter : {-100ms, 0ms, 100ms}){
            const int walked = tiles_before_interrupt(leg, TILE * step + DELAY + jitter, DELAY, 8);
            if (walked != step){
                return std::format("Step {} seen {}ms off read as {}", step, jitter.count(), walked);
            }
        }
    }
    if (tiles_before_interrupt(leg, 50ms, DELAY, 8) != 0){
        return "An interrupt from before the leg should count no tiles.";
    }
    if (tiles_before_interrupt(leg, TILE * 20, DELAY, 3) != 3){
        return "Should not count more tiles than remain.";
    }
    return true;
}

UnitTestResult test_wall_leg_walked_again(){
    ScriptedLeg script;
    script.outcomes = {after_step(2)};
    const LegSpec leg{Direction::NORTH, 6, true, TILE};
    const LegResult result = walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 2 || script.holds[1] != leg_hold(leg, 6)){
        return "Expected the whole leg held again.";
    }
    if (!result.exact || result.interruptions != 1 || script.cleared.size() != 1){
        return "Expected an exact result after one interruption.";
    }
    return true;
}

UnitTestResult test_open_leg_rest_walked(){
    ScriptedLeg script;
    script.outcomes = {after_step(2)};
    const LegSpec leg{Direction::NORTH, 6, false, TILE};
    const LegResult result = walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 2 || script.holds[1] != leg_hold(leg, 4)){
        return "Expected the remaining 4 tiles held.";
    }
    if (result.exact){
        return "An interrupted open leg is an estimate.";
    }
    return true;
}

UnitTestResult test_open_leg_interrupted_at_end(){
    ScriptedLeg script;
    script.outcomes = {after_step(6)};
    const LegSpec leg{Direction::NORTH, 6, false, TILE};
    walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 1){
        return "Nothing was left to walk after the last step.";
    }
    return true;
}

//  An open leg set to stop at an interrupt doesn't walk on from a guess; it
//  says how far the timing puts the player, for the caller to look around.
UnitTestResult test_open_leg_stops(){
    ScriptedLeg script;
    script.outcomes = {after_step(2)};
    LegSpec leg{Direction::NORTH, 6, false, TILE};
    leg.stop_on_interrupt = true;
    const LegResult result = walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 1 || !result.stopped || result.exact || script.cleared.size() != 1){
        return "Expected one hold, the interrupt cleared, and the leg stopped.";
    }
    if (result.tiles != 2){
        return std::format("Expected the timing to put the player 2 tiles in, got {}.", result.tiles);
    }
    return true;
}

//  Console B, 2026-11-01, run 16: held south into a sign, the leg to a wall
//  was interrupted by its message 600ms into every hold, six times.
UnitTestResult test_wall_leg_blocked(){
    ScriptedLeg script;
    script.outcomes = {HoldOutcome{0, 2200ms}, HoldOutcome{0, 600ms}, HoldOutcome{0, 600ms}};
    LegSpec leg{Direction::SOUTH, 23, true, TILE};
    leg.stop_if_blocked = true;
    const LegResult result = walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 2 || !result.stopped){
        return std::format("Expected the leg stopped after 2 holds, got {}.", script.holds.size());
    }
    return true;
}

UnitTestResult test_too_many_interruptions(){
    ScriptedLeg script;
    for (int c = 0; c < 4; c++){
        script.outcomes.push_back(after_step(0));
    }
    const LegSpec leg{Direction::NORTH, 6, true, TILE};
    try{
        walk_leg_with(leg, script.calls(), 3);
    }catch (std::runtime_error&){
        return true;
    }
    return "Expected to give up after 3 interruptions.";
}

//  A watcher that fires on the frame numbered `fires_on` (from 1).
class CountingWatcher : public VisualInferenceCallback{
public:
    explicit CountingWatcher(int fires_on)
        : VisualInferenceCallback("CountingWatcher")
        , m_fires_on(fires_on)
    {}
    int frames() const{
        return m_frames;
    }
    virtual void make_overlays(VideoOverlaySet&) const override{}
    using VisualInferenceCallback::process_frame;
    virtual bool process_frame(const VideoSnapshot&) override{
        return ++m_frames == m_fires_on;
    }

private:
    int m_fires_on;
    int m_frames = 0;
};

UnitTestResult test_watch_along(){
    CountingWatcher watcher(3);
    WatchAlong along(watcher);
    for (int c = 0; c < 5; c++){
        if (along.process_frame(VideoSnapshot())){
            return std::format("WatchAlong stopped the hold on frame {}.", c + 1);
        }
        if (along.fired() != (c >= 2)){
            return std::format("After frame {}, expected fired() {}.", c + 1, c >= 2);
        }
    }
    if (watcher.frames() != 3){
        return std::format("Expected the watcher to see 3 frames, up to its firing; it saw {}.", watcher.frames());
    }
    return true;
}


struct LegWalkerTestCase{
    struct Input{
        std::string name;
        UnitTestResult (*test)();
    };

    struct Expectation{
        std::string_view description;
    };

    Input input;
    Expectation expectation;
};

class Test_LegWalker : public UnitTest{
public:
    explicit Test_LegWalker(LegWalkerTestCase scenario)
        : UnitTest(std::format("Pokemon::LegWalker - {}", scenario.input.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.test();
    }

private:
    LegWalkerTestCase m_scenario;
};

const LegWalkerTestCase LEG_WALKER_CASES[] = {
    {
        .input = {.name = "hold times", .test = test_hold_times},
        .expectation = {.description = "verifies open and wall leg hold durations"},
    },
    {
        .input = {.name = "tiles before an interrupt", .test = test_tiles_before_interrupt},
        .expectation = {.description = "computes traversed tiles before an interrupt occurred"},
    },
    {
        .input = {.name = "a wall leg is walked again", .test = test_wall_leg_walked_again},
        .expectation = {.description = "re-executes entire wall leg following an interrupt"},
    },
    {
        .input = {.name = "an open leg walks the rest", .test = test_open_leg_rest_walked},
        .expectation = {.description = "walks only the remaining tiles on an open leg"},
    },
    {
        .input = {.name = "an open leg interrupted at its end", .test = test_open_leg_interrupted_at_end},
        .expectation = {.description = "does not re-walk an open leg interrupted at its final tile"},
    },
    {
        .input = {.name = "gives up after too many interruptions", .test = test_too_many_interruptions},
        .expectation = {.description = "throws when interrupt count exceeds limit"},
    },
    {
        .input = {.name = "an open leg stops at an interrupt", .test = test_open_leg_stops},
        .expectation = {.description = "aborts remaining movement when stop_on_interrupt is set"},
    },
    {
        .input = {.name = "a wall leg blocked by a sign stops", .test = test_wall_leg_blocked},
        .expectation = {.description = "aborts wall movement when blocked prematurely repeatedly"},
    },
    {
        .input = {.name = "a watcher along a hold never stops it", .test = test_watch_along},
        .expectation = {.description = "WatchAlong keeps its watcher's firing and returns false"},
    },
};

} // namespace


void add_tests_LegWalker(UnitTestDatabase& database){
    for (const auto& scenario : LEG_WALKER_CASES){
        database.add<Test_LegWalker>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
