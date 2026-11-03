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

class Test_OverworldWalk : public UnitTest{
public:
    Test_OverworldWalk(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::OverworldWalk - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};


struct HoldCase{
    std::string name;
    MoveSpeed speed;
    int tiles;
    bool to_wall;
    bool surfing;
    //  How long the D-pad is held.
    Milliseconds hold;
    //  speed_note()'s line, or empty.
    std::string note;
    //  On a map where the player can't run (a Pokemon Center).
    bool walk_only = false;
    //  Whether the leg is held with B.
    bool runs = false;
};

//  An open leg lets go half a tile before its last step would end; a leg
//  to a wall is held two tiles longer. Walking is 267ms a tile, running and
//  surfing 133ms.
const std::vector<HoldCase>& hold_cases(){
    static const std::vector<HoldCase> cases{
        {.name = "walk, open", .speed = MoveSpeed::WALK, .tiles = 5, .to_wall = false, .surfing = false,
            .hold = Milliseconds(4 * 267 + 133), .note = ""},
        {.name = "walk, to a wall", .speed = MoveSpeed::WALK, .tiles = 5, .to_wall = true, .surfing = false,
            .hold = Milliseconds(7 * 267), .note = "[Speed] walk west: 5 tiles in 1335 ms, held 1869 ms"},
        {.name = "run, open", .speed = MoveSpeed::RUN, .tiles = 5, .to_wall = false, .surfing = false,
            .hold = Milliseconds(4 * 133 + 66), .note = "", .runs = true},
        {.name = "run, to a wall", .speed = MoveSpeed::RUN, .tiles = 40, .to_wall = true, .surfing = false,
            .hold = Milliseconds(42 * 133), .note = "[Speed] run west: 40 tiles in 5320 ms, held 5586 ms", .runs = true},
        {.name = "run, indoors: walks", .speed = MoveSpeed::RUN, .tiles = 5, .to_wall = true, .surfing = false,
            .hold = Milliseconds(7 * 267), .note = "[Speed] walk west: 5 tiles in 1335 ms, held 1869 ms", .walk_only = true},
        {.name = "run, indoors, open: walks", .speed = MoveSpeed::RUN, .tiles = 5, .to_wall = false, .surfing = false,
            .hold = Milliseconds(4 * 267 + 133), .note = "", .walk_only = true},
        {.name = "run, surfing to a wall", .speed = MoveSpeed::RUN, .tiles = 3, .to_wall = true, .surfing = true,
            .hold = Milliseconds(5 * 133), .note = ""},
        {.name = "walk, surfing", .speed = MoveSpeed::WALK, .tiles = 3, .to_wall = false, .surfing = true,
            .hold = Milliseconds(2 * 133 + 66), .note = ""},
    };
    return cases;
}

//  Whether `mover` is hold_dpad_running(), which holds B.
bool holds_b(const Pokemon::LegMover<ProControllerContext>& mover){
    using Mover = void (*)(ProControllerContext&, Direction, Milliseconds);
    const Mover* target = mover.target<Mover>();
    return target != nullptr && *target == hold_dpad_running;
}

UnitTestResult check_hold(const HoldCase& test){
    RouteStep step;
    step.direction = Direction::WEST;
    step.tiles = test.tiles;
    step.to_wall = test.to_wall;
    step.surfing = test.surfing;
    step.walk_only = test.walk_only;
    const Milliseconds hold = leg_hold_time(step, test.speed);
    if (hold != test.hold){
        return std::format("Expected a hold of {}ms, got {}ms.", test.hold.count(), hold.count());
    }
    const std::string note = speed_note(step, test.speed);
    if (note != test.note){
        return "Expected the note \"" + test.note + "\", got \"" + note + "\".";
    }
    if (holds_b(leg_mover(step, test.speed)) != test.runs){
        return test.runs ? "Expected the leg held with B." : "Expected the leg held without B.";
    }
    return true;
}

}


void add_tests_OverworldWalk(UnitTestDatabase& database){
    for (const HoldCase& test : hold_cases()){
        database.add<Test_OverworldWalk>("hold: " + test.name, [&test]{ return check_hold(test); });
    }
}



}
}
}
