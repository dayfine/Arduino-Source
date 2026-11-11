/*  Overworld Walk Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_BikeSpinMechanics.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace MovementMechanics;
using namespace OverworldWalkSteps;


namespace{

struct OverworldWalkInput{
    const char* name;
    MoveSpeed speed;
    int tiles;
    bool to_wall;
    bool surfing;
    bool walk_only = false;
};

struct OverworldWalkExpectation{
    Milliseconds hold;
    const char* note;
    bool runs = false;
};

struct OverworldWalkTestCase{
    OverworldWalkInput input;
    OverworldWalkExpectation expectation;
};

//  Whether `mover` is hold_dpad_running(), which holds B.
bool holds_b(const Pokemon::LegMover<ProControllerContext>& mover){
    using Mover = void (*)(ProControllerContext&, Direction, Milliseconds);
    const Mover* target = mover.target<Mover>();
    return target != nullptr && *target == hold_dpad_running;
}

class Test_OverworldWalk : public UnitTest{
public:
    explicit Test_OverworldWalk(OverworldWalkTestCase scenario)
        : UnitTest("PokemonFRLG::OverworldWalk - hold: " + std::string(scenario.input.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        RouteStep step;
        step.direction = Direction::WEST;
        step.tiles = m_scenario.input.tiles;
        step.to_wall = m_scenario.input.to_wall;
        step.surfing = m_scenario.input.surfing;
        step.walk_only = m_scenario.input.walk_only;
        const Milliseconds hold = leg_hold_time(step, m_scenario.input.speed);
        if (hold != m_scenario.expectation.hold){
            return std::format("Expected a hold of {}ms, got {}ms.", m_scenario.expectation.hold.count(), hold.count());
        }
        const std::string note = speed_note(step, m_scenario.input.speed);
        if (note != m_scenario.expectation.note){
            return "Expected the note \"" + std::string(m_scenario.expectation.note) + "\", got \"" + note + "\".";
        }
        if (holds_b(leg_mover(step, m_scenario.input.speed)) != m_scenario.expectation.runs){
            return m_scenario.expectation.runs ? "Expected the leg held with B." : "Expected the leg held without B.";
        }
        return true;
    }

private:
    OverworldWalkTestCase m_scenario;
};

//  An open leg lets go half a tile before its last step would end; a leg
//  to a wall is held two tiles longer. Walking is 267ms a tile, running and
//  surfing 133ms.
const OverworldWalkTestCase OVERWORLD_WALK_CASES[] = {
    {
        .input = {.name = "walk, open", .speed = MoveSpeed::WALK, .tiles = 5, .to_wall = false, .surfing = false},
        .expectation = {.hold = Milliseconds(4 * 267 + 133), .note = ""},
    },
    {
        .input = {.name = "walk, to a wall", .speed = MoveSpeed::WALK, .tiles = 5, .to_wall = true, .surfing = false},
        .expectation = {.hold = Milliseconds(7 * 267), .note = "[Speed] walk west: 5 tiles in 1335 ms, held 1869 ms"},
    },
    {
        .input = {.name = "run, open", .speed = MoveSpeed::RUN, .tiles = 5, .to_wall = false, .surfing = false},
        .expectation = {.hold = Milliseconds(4 * 133 + 66), .note = "", .runs = true},
    },
    {
        .input = {.name = "run, to a wall", .speed = MoveSpeed::RUN, .tiles = 40, .to_wall = true, .surfing = false},
        .expectation = {.hold = Milliseconds(42 * 133), .note = "[Speed] run west: 40 tiles in 5320 ms, held 5586 ms", .runs = true},
    },
    {
        .input = {.name = "run, indoors: walks", .speed = MoveSpeed::RUN, .tiles = 5, .to_wall = true, .surfing = false, .walk_only = true},
        .expectation = {.hold = Milliseconds(7 * 267), .note = "[Speed] walk west: 5 tiles in 1335 ms, held 1869 ms"},
    },
    {
        .input = {.name = "run, indoors, open: walks", .speed = MoveSpeed::RUN, .tiles = 5, .to_wall = false, .surfing = false, .walk_only = true},
        .expectation = {.hold = Milliseconds(4 * 267 + 133), .note = ""},
    },
    {
        .input = {.name = "run, surfing to a wall", .speed = MoveSpeed::RUN, .tiles = 3, .to_wall = true, .surfing = true},
        .expectation = {.hold = Milliseconds(5 * 133), .note = ""},
    },
    {
        .input = {.name = "walk, surfing", .speed = MoveSpeed::WALK, .tiles = 3, .to_wall = false, .surfing = true},
        .expectation = {.hold = Milliseconds(2 * 133 + 66), .note = ""},
    },
    //  Console B, 2026-11-09: on the bike, 1450ms moved 15 tiles and 50ms
    //  (a tap) one; 1736ms, held for "west 16 (open)", went past 16.
    {
        .input = {.name = "bike, open 16", .speed = MoveSpeed::BIKE, .tiles = 16, .to_wall = false, .surfing = false},
        .expectation = {.hold = Milliseconds(1550), .note = ""},
    },
    {
        .input = {.name = "bike, open 1", .speed = MoveSpeed::BIKE, .tiles = 1, .to_wall = false, .surfing = false},
        .expectation = {.hold = Milliseconds(50), .note = ""},
    },
};


struct TurnTapInput{
    std::string name;
    //  SimScript::dropped_turn_taps.
    std::set<int> dropped_turn_taps;
    //  SimWalkerRules::confirm_turn.
    bool confirm_turn = true;
    //  On the Bicycle (SimScript::on_bike, SimWalkerRules::speed).
    bool on_bike = false;
};

struct TurnTapExpectation{
    //  Where the leg ends.
    MapPosition end;
};

struct TurnTapCase{
    TurnTapInput input;
    TurnTapExpectation expectation;
};

const MapPosition TURN_START{"ViridianForest", {13, 28}};
const MapPosition TURN_END{"ViridianForest", {12, 28}};

//  Console B, 2026-11-09 (08.01 run 2): the game missed the turn tap before
//  a one-tile open leg, the leg's hold only turned the player, and the leg
//  was taken as walked. On the sim, a leg west 1 for a player facing north.
class Test_OverworldTurnTap : public UnitTest{
public:
    explicit Test_OverworldTurnTap(TurnTapCase test_case)
        : UnitTest("PokemonFRLG::OverworldWalk - turn tap: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        static const std::vector<MapData> MAPS = overworld_maps();
        RouteStep west;
        west.direction = Direction::WEST;
        west.tiles = 1;
        west.facing_after = Direction::WEST;
        west.end = TURN_END;
        SimScript script;
        script.dropped_turn_taps = m_case.input.dropped_turn_taps;
        SimWalkerRules rules;
        rules.confirm_turn = m_case.input.confirm_turn;
        if (m_case.input.on_bike){
            script.on_bike = true;
            script.timing.tile = BIKE_TILE_TIME;
            rules.speed = MoveSpeed::BIKE;
        }
        OverworldSim sim(MAPS, TURN_START, Direction::NORTH, script);
        OverworldSimWalker walker(sim, rules);
        const RoutePlanning planning{.maps = &MAPS, .costs = default_route_costs(), .start = TURN_START, .label = "Sim"};
        const std::string failure = walker.run({west}, planning);
        if (failure.empty() && sim.state().position == m_case.expectation.end){
            return true;
        }
        return std::format(
            "Expected the leg to end on {}; ended on {}, failure \"{}\".",
            to_string(m_case.expectation.end), to_string(sim.state().position), failure
        );
    }

private:
    TurnTapCase m_case;
};

const TurnTapCase TURN_TAP_CASES[] = {
    {.input = {.name = "taken"}, .expectation = {.end = TURN_END}},
    {.input = {.name = "dropped, tapped again", .dropped_turn_taps = {1}}, .expectation = {.end = TURN_END}},
    {.input = {.name = "dropped twice, tapped again", .dropped_turn_taps = {1, 2}}, .expectation = {.end = TURN_END}},
    {.input = {.name = "dropped, not checked: the leg only turns", .dropped_turn_taps = {1}, .confirm_turn = false}, .expectation = {.end = TURN_START}},
    //  Console B, 2026-11-09 (08.03): on the Bicycle the turn tap took a
    //  step, each one-tile leg went two, and the walk to Route 8 ended on
    //  Route 12.
    {.input = {.name = "on the bike, no turn tap", .on_bike = true}, .expectation = {.end = TURN_END}},
};


struct TapMechanicsInput{
    std::string name;
    //  The player's facing at the start.
    Direction facing = Direction::NORTH;
    //  The presses, on the sim.
    std::function<void(OverworldSim&)> presses;
};

struct TapMechanicsExpectation{
    //  Tiles stepped east.
    int steps = 0;
    Direction facing = Direction::EAST;
    //  Step taps the game ignored.
    int ignored = 0;
};

struct TapMechanicsCase{
    TapMechanicsInput input;
    TapMechanicsExpectation expectation;
};

//  Open floor east of the start: where the taps were measured (console B,
//  2026-11-09, MovementMechanics).
const MapPosition TAP_START{"Route10_PokemonCenter_1F", {3, 4}};

//  The sim's taps east, as the console moved the player for them.
class Test_OverworldTapMechanics : public UnitTest{
public:
    explicit Test_OverworldTapMechanics(TapMechanicsCase test_case)
        : UnitTest("PokemonFRLG::OverworldWalk - tap: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        static const std::vector<MapData> MAPS = overworld_maps();
        OverworldSim sim(MAPS, TAP_START, m_case.input.facing);
        m_case.input.presses(sim);
        const SimState& state = sim.state();
        const TapMechanicsExpectation& expected = m_case.expectation;
        const MapPosition end{TAP_START.map, {TAP_START.point.x + expected.steps, TAP_START.point.y}};
        if (state.position == end && state.facing == expected.facing && state.step_taps_ignored == expected.ignored){
            return true;
        }
        return std::format(
            "Expected {} step(s) east to {} facing {}, {} tap(s) ignored; on {} facing {}, {} ignored.",
            expected.steps, to_string(end), Pokemon::direction_name(expected.facing), expected.ignored,
            to_string(state.position), Pokemon::direction_name(state.facing), state.step_taps_ignored
        );
    }

private:
    TapMechanicsCase m_case;
};

//  `count` taps east of `duration`, each released for `release`.
std::function<void(OverworldSim&)> taps_east(int count, Milliseconds duration, Milliseconds release){
    return [=](OverworldSim& sim){
        for (int c = 0; c < count; c++){
            sim.tap(Direction::EAST, duration, release);
        }
    };
}

//  A turn tap east and a 1000ms wait, then `count` taps east of 66ms,
//  each released for `release`.
std::function<void(OverworldSim&)> turn_then_taps(int count, Milliseconds release){
    return [=](OverworldSim& sim){
        sim.tap(Direction::EAST, TURN_TAP, Milliseconds(1000));
        taps_east(count, Milliseconds(66), release)(sim);
    };
}

//  AutoStory's step_once() east `count` times.
std::function<void(OverworldSim&)> step_once_east(int count){
    return [=](OverworldSim& sim){
        for (int c = 0; c < count; c++){
            sim_step_once(sim, Direction::EAST);
        }
    };
}

const TapMechanicsCase TAP_MECHANICS_CASES[] = {
    {.input = {.name = "133ms facing another way only turns", .presses = taps_east(1, LONGEST_TAP, Milliseconds(700))}, .expectation = {}},
    {.input = {.name = "16ms facing that way steps once", .facing = Direction::EAST, .presses = taps_east(1, Milliseconds(16), Milliseconds(700))}, .expectation = {.steps = 1}},
    {.input = {.name = "3 step taps 566ms apart take 2 steps", .presses = turn_then_taps(3, Milliseconds(500))}, .expectation = {.steps = 2, .ignored = 1}},
    {.input = {.name = "3 step taps 666ms apart take 3 steps", .presses = turn_then_taps(3, Milliseconds(600))}, .expectation = {.steps = 3}},
    {.input = {.name = "3 step taps with no release are one hold", .presses = turn_then_taps(3, Milliseconds(0))}, .expectation = {.steps = 1}},
    {.input = {.name = "step_once 3 times takes 3 steps", .presses = step_once_east(3)}, .expectation = {.steps = 3}},
};

} // namespace


void add_tests_OverworldWalk(UnitTestDatabase& database){
    for (const auto& scenario : OVERWORLD_WALK_CASES){
        database.add<Test_OverworldWalk>(scenario);
    }
    for (const TurnTapCase& item : TURN_TAP_CASES){
        database.add<Test_OverworldTurnTap>(item);
    }
    for (const TapMechanicsCase& item : TAP_MECHANICS_CASES){
        database.add<Test_OverworldTapMechanics>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
