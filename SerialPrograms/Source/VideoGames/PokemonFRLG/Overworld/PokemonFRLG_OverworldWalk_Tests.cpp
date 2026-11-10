/*  Overworld Walk Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace OverworldWalkSteps;


namespace{

struct OverworldWalkInput{
    std::string_view name;
    MoveSpeed speed;
    int tiles;
    bool to_wall;
    bool surfing;
};

struct OverworldWalkExpectation{
    Milliseconds hold;
    std::string_view note;
};

struct OverworldWalkTestCase{
    OverworldWalkInput input;
    OverworldWalkExpectation expectation;
};

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
        const Milliseconds hold = leg_hold_time(step, m_scenario.input.speed);
        if (hold != m_scenario.expectation.hold){
            return std::format("Expected a hold of {}ms, got {}ms.", m_scenario.expectation.hold.count(), hold.count());
        }
        const std::string note = speed_note(step, m_scenario.input.speed);
        if (note != m_scenario.expectation.note){
            return "Expected the note \"" + std::string(m_scenario.expectation.note) + "\", got \"" + note + "\".";
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
        .expectation = {.hold = Milliseconds(4 * 133 + 66), .note = ""},
    },
    {
        .input = {.name = "run, to a wall", .speed = MoveSpeed::RUN, .tiles = 40, .to_wall = true, .surfing = false},
        .expectation = {.hold = Milliseconds(42 * 133), .note = "[Speed] run west: 40 tiles in 5320 ms, held 5586 ms"},
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

} // namespace


void add_tests_OverworldWalk(UnitTestDatabase& database){
    for (const auto& scenario : OVERWORLD_WALK_CASES){
        database.add<Test_OverworldWalk>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
