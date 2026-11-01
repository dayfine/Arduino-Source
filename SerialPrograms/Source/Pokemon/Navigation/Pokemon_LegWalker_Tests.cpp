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
#include "Pokemon_LegWalker.h"
#include "Pokemon_Navigation_Tests.h"

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

//  An open leg set to stop at an interrupt doesn't walk on from a guess.
UnitTestResult test_open_leg_stops(){
    ScriptedLeg script;
    script.outcomes = {after_step(2)};
    LegSpec leg{Direction::NORTH, 6, false, TILE};
    leg.stop_on_interrupt = true;
    const LegResult result = walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 1 || !result.stopped || result.exact || script.cleared.size() != 1){
        return "Expected one hold, the interrupt cleared, and the leg stopped.";
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


class Test_LegWalker : public UnitTest{
public:
    Test_LegWalker(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("Pokemon::LegWalker - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

}


void add_tests_LegWalker(UnitTestDatabase& database){
    database.add<Test_LegWalker>("hold times", test_hold_times);
    database.add<Test_LegWalker>("tiles before an interrupt", test_tiles_before_interrupt);
    database.add<Test_LegWalker>("a wall leg is walked again", test_wall_leg_walked_again);
    database.add<Test_LegWalker>("an open leg walks the rest", test_open_leg_rest_walked);
    database.add<Test_LegWalker>("an open leg interrupted at its end", test_open_leg_interrupted_at_end);
    database.add<Test_LegWalker>("gives up after too many interruptions", test_too_many_interruptions);
    database.add<Test_LegWalker>("an open leg stops at an interrupt", test_open_leg_stops);
    database.add<Test_LegWalker>("a wall leg blocked by a sign stops", test_wall_leg_blocked);
}



}
}
