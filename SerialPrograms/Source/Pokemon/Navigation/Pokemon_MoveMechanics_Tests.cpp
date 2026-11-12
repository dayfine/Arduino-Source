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


struct MoveMechanicsInput{
    std::string_view game;
    MoveMechanics mechanics;
};
struct MoveMechanicsExpectation{
    //  How the walker presses an open leg on foot (the user's pick,
    //  2026-11-11: BDSP taps, FRLG timed holds).
    OpenLegPress open_legs_on_foot;
};
struct MoveMechanicsCase{
    MoveMechanicsInput input;
    MoveMechanicsExpectation expectation;
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

class Test_MoveMechanics : public UnitTest{
public:
    explicit Test_MoveMechanics(MoveMechanicsCase scenario)
        : UnitTest(std::format("Pokemon::MoveMechanics - {}", scenario.input.game)), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const MoveMechanics& game = m_scenario.input.mechanics;
        if (game.turn_tap >= game.step_tap){
            return std::format("The turn tap ({} ms) is not shorter than the step tap ({} ms).",
                game.turn_tap.count(), game.step_tap.count());
        }
        if (std::optional<std::string> slower = not_faster("running", game.run, "walking", game.walk)){
            return *slower;
        }
        if (game.bike){
            if (std::optional<std::string> slower = not_faster("the bike", *game.bike, "running", game.run)){
                return *slower;
            }
        }
        const OpenLegPress expected = m_scenario.expectation.open_legs_on_foot;
        if (game.walk.open_legs != expected || game.run.open_legs != expected){
            return "Open legs on foot are not pressed the way the game's walker presses them.";
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
