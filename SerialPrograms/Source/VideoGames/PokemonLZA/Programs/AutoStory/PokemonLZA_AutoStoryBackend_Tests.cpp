/*  PokemonLZA AutoStory - Story backend Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  face_heading() on a scripted game whose minimap arrow turns only once
 *  the queued turn is done, as on the console: a read before observe()
 *  still shows the facing from before the turn.
 *
 */

#include <cmath>
#include <format>
#include <string>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryBackend_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::Observation;
using Pokemon::ZonePose;


//  A player facing `facing` on the minimap. A stick tap after L turns it
//  by the tap's angle (story_turn()), once the queued inputs are done:
//  observe() waits for them, as PlzaConsoleBackend's does.
class TurningGame : public Pokemon::FakeStoryBackend<ZonePose>{
public:
    TurningGame(double facing, bool turns_take, CancellableScope& scope)
        : FakeStoryBackend({}, scope)
        , m_facing(facing)
        , m_turns_take(turns_take)
    {}

    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override{
        if (m_turns_take){
            m_queued_turn += degrees;
        }
        FakeStoryBackend::stick(degrees, duration, release, pace);
    }
    virtual Observation<ZonePose> observe() override{
        m_facing = std::fmod(std::fmod(m_facing + m_queued_turn, 360) + 360, 360);
        m_queued_turn = 0;
        return FakeStoryBackend::observe();
    }

    //  The arrow as shown now: the queued turn not in yet.
    double arrow() const{
        return m_facing;
    }

private:
    double m_facing;
    const bool m_turns_take;
    double m_queued_turn = 0;
};


struct FaceHeadingCase{
    std::string name;
    struct Input{
        double facing;
        double heading;
        //  False: the player stands still whatever the stick does.
        bool turns_take = true;
    };
    Input input;
    struct Expectation{
        //  Empty: faced.
        std::string failure;
    };
    Expectation expectation;
};

const FaceHeadingCase FACE_HEADING_CASES[] = {
    {
        .name = "run 10 of 03.08: from 135 to west",
        .input = {.facing = 135, .heading = 270},
        .expectation = {.failure = ""},
    },
    {
        .name = "from the opposite way",
        .input = {.facing = 90, .heading = 270},
        .expectation = {.failure = ""},
    },
    {
        .name = "across north",
        .input = {.facing = 350, .heading = 20},
        .expectation = {.failure = ""},
    },
    {
        .name = "the player does not turn",
        .input = {.facing = 135, .heading = 270, .turns_take = false},
        .expectation = {.failure = "heading_missed"},
    },
};

UnitTestResult check_face_heading(const FaceHeadingCase& test, CancellableScope& scope){
    TurningGame game(test.input.facing, test.input.turns_take, scope);
    const Pokemon::Outcome outcome = face_heading(game, [&game](){ return game.arrow(); }, test.input.heading);
    if (outcome.failure != test.expectation.failure){
        return std::format(
            "expected failure \"{}\"; got \"{}\", facing {:.0f}",
            test.expectation.failure, outcome.failure, game.arrow()
        );
    }
    return true;
}

class FaceHeadingTest : public UnitTest{
public:
    explicit FaceHeadingTest(FaceHeadingCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - face_heading: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_face_heading(m_case, scope);
    }
private:
    FaceHeadingCase m_case;
};

} // namespace


void add_tests_AutoStoryBackend(UnitTestDatabase& database){
    for (const FaceHeadingCase& test : FACE_HEADING_CASES){
        database.add<FaceHeadingTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
