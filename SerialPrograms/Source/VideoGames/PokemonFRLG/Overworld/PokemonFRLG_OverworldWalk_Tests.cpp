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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

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
};


struct TurnTapInput{
    std::string name;
    //  SimScript::dropped_turn_taps.
    std::set<int> dropped_turn_taps;
    //  SimWalkerRules::confirm_turn.
    bool confirm_turn = true;
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

//  Console B, 2026-11-09 (00.28 run 2): the game missed the turn tap before
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
};

} // namespace


void add_tests_OverworldWalk(UnitTestDatabase& database){
    for (const auto& scenario : OVERWORLD_WALK_CASES){
        database.add<Test_OverworldWalk>(scenario);
    }
    for (const TurnTapCase& item : TURN_TAP_CASES){
        database.add<Test_OverworldTurnTap>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
