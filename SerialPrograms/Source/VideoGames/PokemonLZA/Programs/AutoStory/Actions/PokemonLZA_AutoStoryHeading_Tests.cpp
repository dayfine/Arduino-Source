/*  PokemonLZA AutoStory - Heading Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  face_heading() on a scripted game whose arrow turns only once the queued
 *  turn is done (observe() waits), as on the console.
 *
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryHeading.h"
#include "PokemonLZA_AutoStoryHeading_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{



//  Below this, a turn lands `small_turn_scale` times as far as asked.
constexpr double SMALL_TURN = 15;

class TurningGame : public Pokemon::FakeStoryBackend{
public:
    TurningGame(double facing, bool turns_take, size_t stale_reads, double small_turn_scale, CancellableScope& scope)
        : FakeStoryBackend({}, scope)
        , m_facing(facing)
        , m_shown(facing)
        , m_turns_take(turns_take)
        , m_stale_reads(stale_reads)
        , m_small_turn_scale(small_turn_scale)
    {}

    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override{
        if (m_turns_take){
            m_queued_turn += std::fabs(degrees) < SMALL_TURN ? degrees * m_small_turn_scale : degrees;
        }
        FakeStoryBackend::stick(degrees, duration, release, pace);
    }
    virtual ScreenObservation observe() override{
        if (m_queued_turn != 0){
            m_shown = m_facing;
            m_stale_left = m_stale_reads;
        }
        m_facing = std::fmod(std::fmod(m_facing + m_queued_turn, 360) + 360, 360);
        m_queued_turn = 0;
        return FakeStoryBackend::observe();
    }

    //  The queued turn not in yet; the old facing while the feed lags.
    double arrow(){
        if (m_stale_left > 0){
            m_stale_left--;
            return m_shown;
        }
        return m_facing;
    }

    double facing() const{
        return m_facing;
    }

private:
    double m_facing;
    double m_shown;
    const bool m_turns_take;
    const size_t m_stale_reads;
    const double m_small_turn_scale;
    size_t m_stale_left = 0;
    double m_queued_turn = 0;
};


double heading_error(double facing, double heading){
    const double error = std::fmod(std::fabs(facing - heading), 360.0);
    return std::min(error, 360 - error);
}


struct FaceHeadingCase{
    std::string name;
    struct Input{
        double facing;
        double heading;
        //  False: the player stands still whatever the stick does.
        bool turns_take = true;
        //  Reads after each turn that still show the old facing.
        size_t stale_reads = 0;
        double small_turn_scale = 1;
    };
    Input input;
    struct Expectation{
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
        .name = "across north",
        .input = {.facing = 350, .heading = 20},
        .expectation = {.failure = ""},
    },
    {
        .name = "8 off is not facing it: 03.09 read 272 for 264 and walked right of the gate",
        .input = {.facing = 272, .heading = 264, .turns_take = false},
        .expectation = {.failure = "heading_missed"},
    },
    {
        //  Korean 03.08 catch_more_species read 161, 163, 269, 9, 9 for 265:
        //  each reading one turn late (console B video, 2026-11-17 23:58).
        .name = "a lagging feed: from 161 to 265",
        .input = {.facing = 161, .heading = 265, .stale_reads = 1},
        .expectation = {.failure = ""},
    },
    {
        //  CHS 03.08 read 248.9, 259.1, 249.1, 259.3 for 254: each 5 deg
        //  correction turned 10 (console B, 2026-11-18 02:03).
        .name = "small turns overshoot: from 249 to 254",
        .input = {.facing = 249, .heading = 254, .small_turn_scale = 2},
        .expectation = {.failure = ""},
    },
};

//  Faced: the player really faces the heading, not only the arrow read.
constexpr double FACED_TOLERANCE = 4;

UnitTestResult check_face_heading(const FaceHeadingCase& test, CancellableScope& scope){
    TurningGame game(test.input.facing, test.input.turns_take, test.input.stale_reads, test.input.small_turn_scale, scope);
    const std::optional<Pokemon::StoryFailure> outcome = face_heading(game, [&game](){ return game.arrow(); }, test.input.heading);
    if ((outcome ? outcome->code : std::string()) != test.expectation.failure){
        return std::format(
            "expected failure \"{}\"; got \"{}\", facing {:.0f}",
            test.expectation.failure, outcome ? outcome->code : std::string(), game.facing()
        );
    }
    if (!outcome && heading_error(game.facing(), test.input.heading) > FACED_TOLERANCE){
        return std::format("faced: the player faces {:.0f}, not {:.0f}", game.facing(), test.input.heading);
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


void add_tests_AutoStoryHeading(UnitTestDatabase& database){
    for (const FaceHeadingCase& test : FACE_HEADING_CASES){
        database.add<FaceHeadingTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
