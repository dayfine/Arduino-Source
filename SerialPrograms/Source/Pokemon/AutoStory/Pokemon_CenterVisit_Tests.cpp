/*  Center Visit Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  run_center_visit() on a fake game: the order of its calls, which
 *  settle follows each door, and the two guards that fail a visit.
 *
 */

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_CenterVisit.h"
#include "Pokemon_CenterVisit_Tests.h"
#include "Pokemon_TestExpect.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{


//  A game whose doors and reads are scripted. It writes each call down
//  in `calls`, in order: "in settle_door inside heal ...".
class FakeCenterVisitIO : public CenterVisitIO{
public:
    FakeCenterVisitIO(WarpSeen in, std::vector<CenterPlace> places, WarpSeen out, std::string& calls)
        : m_in(std::move(in))
        , m_places(std::move(places))
        , m_out(std::move(out))
        , m_calls(calls)
    {}

    virtual WarpSeen walk_in() override{
        note("in");
        return m_in;
    }
    virtual void line_up_and_walk_in() override{
        note("line_up");
    }
    virtual CenterPlace place() override{
        const CenterPlace place = m_next < m_places.size() ? m_places[m_next] : CenterPlace::UNKNOWN;
        m_next++;
        note(place == CenterPlace::INSIDE ? "inside" : place == CenterPlace::OUTSIDE ? "outside" : "unknown");
        return place;
    }
    virtual void heal() override{
        note("heal");
    }
    virtual WarpSeen walk_out() override{
        note("out");
        return m_out;
    }
    virtual void settle(SettleKind kind, std::string_view) override{
        note(kind == SettleKind::DOOR ? "settle_door" : kind == SettleKind::AFTER_WARP ? "settle_warp" : "settle_talk");
    }
    virtual void log(std::string_view) override{}

private:
    void note(std::string_view call){
        if (!m_calls.empty()){
            m_calls += " ";
        }
        m_calls += call;
    }

    const WarpSeen m_in;
    const std::vector<CenterPlace> m_places;
    const WarpSeen m_out;
    size_t m_next = 0;
    std::string& m_calls;
};

const WarpSeen FADE{.fade = WarpFade::BLACK};
const WarpSeen NO_FADE{};


struct VisitTestCase{
    struct Input{
        WarpSeen in;
        //  What place() reads, call by call; UNKNOWN after the last.
        std::vector<CenterPlace> places;
        WarpSeen out;
    };

    struct Expectation{
        std::string calls;
        //  The Outcome's failure; nothing for a visit that succeeds.
        std::optional<std::string> failure = std::nullopt;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_Visit : public UnitTest{
public:
    explicit Test_Visit(VisitTestCase test_case)
        : UnitTest(std::format("Pokemon::CenterVisit - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::string calls;
        FakeCenterVisitIO io(m_case.input.in, m_case.input.places, m_case.input.out, calls);
        const Outcome outcome = run_center_visit("Heal", io);
        const std::string got = outcome.succeeded() ? "succeeded" : outcome.failure;
        const UnitTestResult result = expect_equal("outcome", got, m_case.expectation.failure.value_or("succeeded"));
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
        return expect_equal("calls", calls, m_case.expectation.calls);
    }

private:
    const VisitTestCase m_case;
};

using P = CenterPlace;

const VisitTestCase VISIT_CASES[] = {
    {
        .name = "in by the fade, healed, out by the fade",
        .input = {.in = FADE, .places = {P::INSIDE, P::OUTSIDE}, .out = FADE},
        .expectation = {.calls = "in settle_door inside heal out settle_door outside"},
    },
    {
        .name = "no fade in, but the inside shows: no lineup",
        .input = {.in = NO_FADE, .places = {P::INSIDE, P::INSIDE, P::OUTSIDE}, .out = FADE},
        .expectation = {.calls = "in inside settle_door inside heal out settle_door outside"},
    },
    {
        .name = "no fade in and no inside: lines up with the door",
        .input = {.in = NO_FADE, .places = {P::OUTSIDE, P::INSIDE, P::OUTSIDE}, .out = FADE},
        .expectation = {.calls = "in outside line_up settle_door inside heal out settle_door outside"},
    },
    {
        .name = "outside after the way in: fails before the nurse",
        .input = {.in = FADE, .places = {P::OUTSIDE}, .out = FADE},
        .expectation = {.calls = "in settle_door outside", .failure = std::string(CENTER_NOT_ENTERED)},
    },
    {
        .name = "no fade out yet: the settle waits for the warp",
        .input = {.in = FADE, .places = {P::INSIDE, P::OUTSIDE}, .out = NO_FADE},
        .expectation = {.calls = "in settle_door inside heal out settle_warp outside"},
    },
    {
        .name = "still inside after the way out: fails",
        .input = {.in = FADE, .places = {P::INSIDE, P::INSIDE}, .out = FADE},
        .expectation = {.calls = "in settle_door inside heal out settle_door inside", .failure = std::string(CENTER_NOT_LEFT)},
    },
    {
        .name = "a game that cannot read the place: no guard fails",
        .input = {.in = FADE, .places = {}, .out = FADE},
        .expectation = {.calls = "in settle_door unknown heal out settle_door unknown"},
    },
};

} // namespace


void add_tests_CenterVisit(UnitTestDatabase& database){
    for (const VisitTestCase& item : VISIT_CASES){
        database.add<Test_Visit>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
