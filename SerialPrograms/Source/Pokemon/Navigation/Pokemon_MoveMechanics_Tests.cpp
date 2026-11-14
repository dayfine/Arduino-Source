/*  Move Mechanics Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_MoveMechanics.h"
#include "Pokemon_MoveMechanics_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{
namespace{


struct MoveMechanicsCase{
    struct Input{
        std::string_view game;
        MoveMechanics mechanics;
    };
    struct Expectation{
        //  How the walker presses an open leg on foot (BDSP taps, FRLG
        //  timed holds).
        OpenLegPress open_legs_on_foot;
    };
    Input input;
    Expectation expectation;
};

//  Why `faster` is not faster than `slower` on both axes, if it is not.
std::optional<std::string> not_faster(std::string_view faster_name, const Gait& faster, std::string_view slower_name, const Gait& slower){
    if (faster.tile.across >= slower.tile.across || faster.tile.up_down >= slower.tile.up_down){
        return std::format("{} ({}/{} ms) is not faster than {} ({}/{} ms).",
            faster_name, faster.tile.across.count(), faster.tile.up_down.count(),
            slower_name, slower.tile.across.count(), slower.tile.up_down.count());
    }
    return std::nullopt;
}

//  Why the turn tap would step on foot in `gait`, if it would: a turn tap
//  must be shorter than the gait's step tap.
std::optional<std::string> turn_steps(std::string_view gait_name, const Gait& gait, Milliseconds turn_tap){
    if (turn_tap >= gait.tap.hold){
        return std::format("The turn tap ({} ms) is not shorter than the {} step tap ({} ms).",
            turn_tap.count(), gait_name, gait.tap.hold.count());
    }
    return std::nullopt;
}

//  Why a gait's step tap is unusable, if it is: both its press and its
//  release must take time.
std::optional<std::string> empty_tap(std::string_view gait_name, const Gait& gait){
    if (gait.tap.hold <= Milliseconds(0) || gait.tap.release <= Milliseconds(0)){
        return std::format("The {} step tap is {} ms pressed, {} ms released; both must be over 0.",
            gait_name, gait.tap.hold.count(), gait.tap.release.count());
    }
    return std::nullopt;
}

std::string_view open_leg_press_name(OpenLegPress press){
    return press == OpenLegPress::STEP_TAPS ? "step taps" : "a timed hold";
}

//  Why `gait` presses its open legs other than `expected`, if it does.
std::optional<std::string> wrong_open_legs(std::string_view gait_name, const Gait& gait, OpenLegPress expected){
    if (gait.open_legs != expected){
        return std::format("Open legs {} are pressed as {}, expected {}.",
            gait_name, open_leg_press_name(gait.open_legs), open_leg_press_name(expected));
    }
    return std::nullopt;
}

class Test_MoveMechanics : public UnitTest{
public:
    explicit Test_MoveMechanics(MoveMechanicsCase scenario)
        : UnitTest(std::format("Pokemon::MoveMechanics - {}", scenario.input.game)), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const MoveMechanics& game = m_scenario.input.mechanics;
        const OpenLegPress expected = m_scenario.expectation.open_legs_on_foot;
        const std::optional<std::string> failures[] = {
            empty_tap("walking", game.walk),
            empty_tap("running", game.run),
            game.bike ? empty_tap("bike", *game.bike) : std::nullopt,
            turn_steps("walking", game.walk, game.turn_tap),
            turn_steps("running", game.run, game.turn_tap),
            not_faster("running", game.run, "walking", game.walk),
            game.bike ? not_faster("the bike", *game.bike, "running", game.run) : std::nullopt,
            wrong_open_legs("walking", game.walk, expected),
            wrong_open_legs("running", game.run, expected),
        };
        for (const std::optional<std::string>& failure : failures){
            if (failure){
                return *failure;
            }
        }
        return true;
    }
private:
    MoveMechanicsCase m_scenario;
};

const MoveMechanicsCase MOVE_MECHANICS_CASES[] = {
    {
        .input = {.game = "BDSP", .mechanics = BDSP_MOVE_MECHANICS},
        .expectation = {.open_legs_on_foot = OpenLegPress::STEP_TAPS},
    },
    {
        .input = {.game = "FRLG", .mechanics = FRLG_MOVE_MECHANICS},
        .expectation = {.open_legs_on_foot = OpenLegPress::TIMED_HOLD},
    },
};


} // namespace

void add_tests_MoveMechanics(UnitTestDatabase& database){
    for (const MoveMechanicsCase& scenario : MOVE_MECHANICS_CASES){
        database.add<Test_MoveMechanics>(scenario);
    }
}


} // namespace Pokemon
} // namespace PokemonAutomation
