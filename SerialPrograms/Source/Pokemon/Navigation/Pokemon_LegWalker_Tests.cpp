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
constexpr Milliseconds WINDOW = 900ms;
constexpr Milliseconds NO_WINDOW = 0ms;

//  Holds that report the scripted outcomes in order, then run out.
struct ScriptedLeg{
    std::deque<std::optional<HoldOutcome>> outcomes;
    Milliseconds delay = DELAY;
    std::vector<Milliseconds> holds;
    std::vector<Milliseconds> windows;
    std::vector<size_t> cleared;
    size_t unwatched = 0;

    LegCalls calls(){
        LegCalls calls;
        calls.hold = [this](Milliseconds hold){
            holds.push_back(hold);
            windows.push_back(NO_WINDOW);
            unwatched++;
        };
        calls.hold_watching = [this](Milliseconds hold, Milliseconds window) -> std::optional<HoldOutcome>{
            holds.push_back(hold);
            windows.push_back(window);
            if (outcomes.empty()){
                return std::nullopt;
            }
            std::optional<HoldOutcome> outcome = outcomes.front();
            outcomes.pop_front();
            return outcome;
        };
        calls.clear = [this](size_t index){ cleared.push_back(index); };
        calls.detection_delay = [this](size_t){ return delay; };
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
    const LegSpec open{Direction::EAST, 4, false, TILE, NO_WINDOW};
    const LegSpec wall{Direction::EAST, 4, true, TILE, NO_WINDOW};
    if (leg_hold(open, 4) != TILE * 3 + TILE / 2){
        return std::format("Open leg: {}ms", leg_hold(open, 4).count());
    }
    if (leg_hold(wall, 4) != TILE * 6){
        return std::format("Wall leg: {}ms", leg_hold(wall, 4).count());
    }
    return true;
}

UnitTestResult test_tiles_before_interrupt(){
    const LegSpec leg{Direction::EAST, 8, false, TILE, NO_WINDOW};
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
    const LegSpec leg{Direction::NORTH, 6, true, TILE, NO_WINDOW};
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
    const LegSpec leg{Direction::NORTH, 6, false, TILE, NO_WINDOW};
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
    const LegSpec leg{Direction::NORTH, 6, false, TILE, NO_WINDOW};
    walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 1){
        return "Nothing was left to walk after the last step.";
    }
    return true;
}

UnitTestResult test_too_many_interruptions(){
    ScriptedLeg script;
    for (int c = 0; c < 4; c++){
        script.outcomes.push_back(after_step(0));
    }
    const LegSpec leg{Direction::NORTH, 6, true, TILE, NO_WINDOW};
    try{
        walk_leg_with(leg, script.calls(), 3);
    }catch (std::runtime_error&){
        return true;
    }
    return "Expected to give up after 3 interruptions.";
}


//  Tile by tile: the outcome of each press in turn, nothing for one that
//  ran out.
UnitTestResult test_steps_back_to_interrupt(){
    const LegSpec leg{Direction::WEST, 4, false, TILE, WINDOW};
    //  Seen in the window of the step that set it off, however slow.
    for (Milliseconds elapsed : {TILE, TILE + 100ms, TILE + 600ms, WINDOW - 1ms}){
        if (steps_back_to_interrupt(leg, elapsed, DELAY) != 0){
            return std::format("Seen {}ms after a press: should be that press's step.", elapsed.count());
        }
    }
    //  Seen before the press could have landed: the previous step's.
    for (Milliseconds elapsed : {0ms, 100ms, TILE - 1ms}){
        if (steps_back_to_interrupt(leg, elapsed, DELAY) != 1){
            return std::format("Seen {}ms after a press: should be the previous step.", elapsed.count());
        }
    }
    //  Slower than a window: counted back by whole windows.
    if (steps_back_to_interrupt(leg, TILE + 100ms, 2000ms) != 2){
        return "A 2s delay should reach back two windows.";
    }
    return true;
}

//  Console B, 2026-11-01: a battle on the 3rd tile of "west 4", seen only
//  after the hold, was taken for the end of the leg.
UnitTestResult test_tile_by_tile_battle_mid_leg(){
    ScriptedLeg script;
    script.outcomes = {std::nullopt, std::nullopt, HoldOutcome{0, TILE + 650ms}};
    const LegSpec leg{Direction::WEST, 4, false, TILE, WINDOW};
    const LegResult result = walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 4){
        return std::format("Expected 4 presses, got {}.", script.holds.size());
    }
    for (size_t c = 0; c < script.holds.size(); c++){
        if (script.holds[c] != leg_hold(leg, 1) || script.windows[c] != WINDOW){
            return "Each press should be one tile, watched for the window.";
        }
    }
    if (!result.exact || result.interruptions != 1 || script.cleared.size() != 1){
        return "Expected an exact result after one interruption.";
    }
    return true;
}

//  A battle seen early in the next window: that press was swallowed, so the
//  tile is walked again.
UnitTestResult test_tile_by_tile_battle_seen_next_window(){
    ScriptedLeg script;
    script.outcomes = {std::nullopt, std::nullopt, std::nullopt, HoldOutcome{0, 100ms}};
    const LegSpec leg{Direction::SOUTH, 4, false, TILE, WINDOW};
    const LegResult result = walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 5){
        return std::format("Expected the 4th tile pressed again (5 presses), got {}.", script.holds.size());
    }
    if (!result.exact){
        return "A battle placed by its window is exact.";
    }
    return true;
}

//  The first tile's battle, seen early in the second window, costs the
//  second press; one seen just after the first press came from before the
//  leg, and the player hasn't moved.
UnitTestResult test_tile_by_tile_battle_on_first_tile(){
    ScriptedLeg script;
    script.outcomes = {std::nullopt, HoldOutcome{0, 50ms}};
    const LegSpec leg{Direction::SOUTH, 7, false, TILE, WINDOW};
    walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 8){
        return std::format("Expected 7 tiles in 8 presses, got {}.", script.holds.size());
    }
    ScriptedLeg before;
    before.outcomes = {HoldOutcome{0, 50ms}};
    walk_leg_with(leg, before.calls(), 5);
    if (before.holds.size() != 8){
        return std::format("Expected 7 tiles in 8 presses after an interrupt from before the leg, got {}.", before.holds.size());
    }
    return true;
}

//  A battle at the last tile ends the leg.
UnitTestResult test_tile_by_tile_battle_on_last_tile(){
    ScriptedLeg script;
    script.outcomes = {std::nullopt, std::nullopt, std::nullopt, HoldOutcome{0, TILE + 300ms}};
    const LegSpec leg{Direction::WEST, 4, false, TILE, WINDOW};
    const LegResult result = walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 4 || result.tiles != 4){
        return std::format("Expected the leg done in 4 presses, got {}.", script.holds.size());
    }
    return true;
}

//  An interrupt slower than the window is placed by its delay: a guess.
UnitTestResult test_tile_by_tile_slow_interrupt(){
    ScriptedLeg script;
    script.delay = 2000ms;
    script.outcomes = {std::nullopt, std::nullopt, std::nullopt, HoldOutcome{0, TILE + 100ms}};
    const LegSpec leg{Direction::WEST, 5, false, TILE, WINDOW};
    const LegResult result = walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 7){
        return std::format("Expected the 2nd step's battle to cost 2 presses (7), got {}.", script.holds.size());
    }
    if (result.exact){
        return "An interrupt slower than the window is an estimate.";
    }
    return true;
}

//  The step onto a warp isn't watched: its fade looks like a flash.
UnitTestResult test_tile_by_tile_warp_not_watched(){
    ScriptedLeg script;
    LegSpec leg{Direction::NORTH, 4, false, TILE, WINDOW};
    leg.warps = true;
    walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 4 || script.unwatched != 1 || script.windows.back() != NO_WINDOW){
        return std::format("Expected 3 watched presses and an unwatched one, got {} and {}.", script.holds.size() - script.unwatched, script.unwatched);
    }
    return true;
}

//  A leg to a wall is still held through, window or not.
UnitTestResult test_wall_leg_held_through(){
    ScriptedLeg script;
    const LegSpec leg{Direction::NORTH, 5, true, TILE, WINDOW};
    walk_leg_with(leg, script.calls(), 5);
    if (script.holds.size() != 1 || script.holds[0] != leg_hold(leg, 5)){
        return "Expected one hold to the wall.";
    }
    return true;
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
    database.add<Test_LegWalker>("steps back to an interrupt", test_steps_back_to_interrupt);
    database.add<Test_LegWalker>("tile by tile: a battle mid-leg", test_tile_by_tile_battle_mid_leg);
    database.add<Test_LegWalker>("tile by tile: a battle seen in the next window", test_tile_by_tile_battle_seen_next_window);
    database.add<Test_LegWalker>("tile by tile: a battle on the first tile", test_tile_by_tile_battle_on_first_tile);
    database.add<Test_LegWalker>("tile by tile: a battle on the last tile", test_tile_by_tile_battle_on_last_tile);
    database.add<Test_LegWalker>("tile by tile: an interrupt slower than the window", test_tile_by_tile_slow_interrupt);
    database.add<Test_LegWalker>("a wall leg is held through", test_wall_leg_held_through);
    database.add<Test_LegWalker>("tile by tile: the step onto a warp isn't watched", test_tile_by_tile_warp_not_watched);
}



}
}
