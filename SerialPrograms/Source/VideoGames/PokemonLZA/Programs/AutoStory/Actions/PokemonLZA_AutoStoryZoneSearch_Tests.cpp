/*  PokemonLZA AutoStory - Zone search Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryZoneSearch.h"
#include "PokemonLZA_AutoStoryZoneStart.h"
#include "PokemonLZA_AutoStoryZoneSearch_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  8 camera steps to a full turn, as catch_in_zone() scans.
constexpr size_t STEPS = 8;

constexpr ZoneLook NONE = ZoneLook::NO_PLATE;
constexpr ZoneLook CAUGHT = ZoneLook::ALL_CAUGHT;
constexpr ZoneLook NEW = ZoneLook::NOT_CAUGHT;
constexpr ZoneLook BATTLE = ZoneLook::WILD_BATTLE;

constexpr ZoneMove TURN = ZoneMove::TURN;
constexpr ZoneMove CATCH = ZoneMove::CATCH;
constexpr ZoneMove FIGHT = ZoneMove::FIGHT;
constexpr ZoneMove WALK = ZoneMove::WALK;
constexpr ZoneMove PAUSE = ZoneMove::PAUSE;

constexpr ZoneLeg TOWARD = ZoneLeg::TOWARD;
constexpr ZoneLeg AWAY = ZoneLeg::AWAY;

//  A round of legs with no distance read: each way out, then back.
const std::vector<ZoneLeg> ROUND = {AWAY, TOWARD, TOWARD, AWAY};

//  `scans` full turns of looks that find nothing new (no plate and caught
//  plates in turn), after `before`.
std::vector<ZoneLook> empty_scans(size_t scans, std::vector<ZoneLook> before = {}){
    for (size_t i = 0; i < scans * STEPS; i++){
        before.push_back(i % 2 == 0 ? NONE : CAUGHT);
    }
    return before;
}

//  The moves of full turns that find nothing new, one per entry of `ends`:
//  7 turns, then the entry; after `before`.
std::vector<ZoneMove> empty_scan_moves(const std::vector<ZoneMove>& ends, std::vector<ZoneMove> before = {}){
    for (ZoneMove end : ends){
        before.insert(before.end(), STEPS - 1, TURN);
        before.push_back(end);
    }
    return before;
}

template <typename Type>
std::vector<Type> operator+(std::vector<Type> front, const std::vector<Type>& back){
    front.insert(front.end(), back.begin(), back.end());
    return front;
}

template <typename Type>
std::vector<Type> repeated(const std::vector<Type>& values, size_t times){
    std::vector<Type> out;
    for (size_t i = 0; i < times; i++){
        out.insert(out.end(), values.begin(), values.end());
    }
    return out;
}

//  The 8 walks of a round with no read, then its pause.
const std::vector<ZoneMove> ROUND_ENDS = {WALK, WALK, WALK, WALK, PAUSE};


struct SearchCase{
    std::string name;
    struct Input{
        std::vector<ZoneLook> looks;
        //  The agreed marker distance read before each leg (none: no read).
        std::optional<int> distance;
        //  Each catch and fight ends with a walk back that reaches the
        //  search spot (ZoneSearch::at_spot()).
        bool back_at_spot_after_catch = false;
    };
    Input input;
    struct Expectation{
        std::vector<ZoneMove> moves;
        std::vector<ZoneLeg> legs;
        //  The furthest the search got from its start, in legs either way.
        int furthest = 0;
        ZoneOffset end;
    };
    Expectation expectation;
};

const SearchCase SEARCH_CASES[] = {
    {
        .name = "a new species locked on at the third camera step is caught",
        .input = {.looks = {CAUGHT, NONE, NEW}},
        .expectation = {.moves = {TURN, TURN, CATCH}},
    },
    {
        .name = "a catch or a wild battle starts the scan over at the first camera step",
        .input = {.looks = empty_scans(1, {NONE, NEW, CAUGHT, BATTLE})},
        .expectation = {.moves = empty_scan_moves({WALK}, {TURN, CATCH, TURN, FIGHT}), .legs = {AWAY}, .furthest = 1, .end = {.toward = -1}},
    },
    {
        .name = "a full turn with nothing new walks one leg, then scans again",
        .input = {.looks = empty_scans(1, {}), .distance = 130},
        .expectation = {.moves = {TURN, TURN, TURN, TURN, TURN, TURN, TURN, WALK}, .legs = {AWAY}, .furthest = 1, .end = {.toward = -1}},
    },
    {
        .name = "with no distance read each leg out is walked back before the next, then the search pauses",
        .input = {.looks = empty_scans(10)},
        .expectation = {.moves = empty_scan_moves(repeated(ROUND_ENDS, 2)), .legs = repeated(ROUND, 2), .furthest = 1, .end = {}},
    },
    {
        .name = "at a distance of 60 or less no leg goes toward the objective",
        .input = {.looks = empty_scans(8), .distance = 55},
        .expectation = {
            .moves = empty_scan_moves({WALK, WALK, WALK, WALK, WALK, WALK, WALK, WALK}),
            .legs = {AWAY, AWAY, AWAY, AWAY, AWAY, AWAY, AWAY, AWAY},
            .furthest = 8, .end = {.toward = -8},
        },
    },
    {
        .name = "at a distance over 60 legs toward the objective are walked",
        .input = {.looks = empty_scans(5), .distance = 61},
        .expectation = {.moves = empty_scan_moves(ROUND_ENDS), .legs = ROUND, .furthest = 1, .end = {}},
    },
    {
        .name = "a catch walked back to the search spot starts the legs from the spot again",
        .input = {.looks = empty_scans(2, empty_scans(1, {}) + std::vector<ZoneLook>{NEW}), .back_at_spot_after_catch = true},
        .expectation = {
            .moves = empty_scan_moves({WALK, WALK}, empty_scan_moves({WALK}, {}) + std::vector<ZoneMove>{CATCH}),
            .legs = {AWAY, TOWARD, AWAY}, .furthest = 1, .end = {},
        },
    },
};

template <typename Type>
std::string text(const std::vector<Type>& values){
    std::string out;
    for (Type value : values){
        out += std::format("{}{}", out.empty() ? "" : ",", (int)value);
    }
    return std::format("{{{}}}", out);
}

std::string text(const ZoneOffset& offset){
    return std::format("({} toward, {} right)", offset.toward, offset.right);
}

//  catch_in_zone()'s loop: each look decides a move; a WALK takes a leg.
UnitTestResult check_search(const SearchCase& test){
    ZoneSearch search(STEPS);
    std::vector<ZoneMove> moves;
    std::vector<ZoneLeg> legs;
    int furthest = 0;
    for (ZoneLook look : test.input.looks){
        const ZoneMove move = search.look(look);
        moves.push_back(move);
        if (test.input.back_at_spot_after_catch && (move == ZoneMove::CATCH || move == ZoneMove::FIGHT)){
            search.at_spot();
        }
        if (move != ZoneMove::WALK){
            continue;
        }
        legs.push_back(search.leg(test.input.distance));
        const ZoneOffset at = search.offset();
        furthest = std::max({furthest, std::abs(at.toward), std::abs(at.right)});
    }
    const SearchCase::Expectation& expected = test.expectation;
    if (moves != expected.moves || legs != expected.legs || furthest != expected.furthest || search.offset() != expected.end){
        return std::format(
            "expected moves {}, legs {}, furthest {}, end {}; got moves {}, legs {}, furthest {}, end {}",
            text(expected.moves), text(expected.legs), expected.furthest, text(expected.end),
            text(moves), text(legs), furthest, text(search.offset())
        );
    }
    return true;
}


class SearchTest : public UnitTest{
public:
    explicit SearchTest(SearchCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - ZoneSearch: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_search(m_case);
    }
private:
    SearchCase m_case;
};


constexpr ZoneStartMove WALK_TO_SPOT = ZoneStartMove::WALK_TO_SPOT;
constexpr ZoneStartMove SEARCH = ZoneStartMove::SEARCH;
constexpr ZoneStartMove ROCKS_NOT_LEFT = ZoneStartMove::ROCKS_NOT_LEFT;
constexpr ZoneStartMove SPOT_NOT_REACHED = ZoneStartMove::SPOT_NOT_REACHED;

struct StartCase{
    std::string name;
    struct Input{
        //  The agreed marker distance after the leg off the rocks, then
        //  after the marker walk (none: no read taken).
        std::vector<std::optional<int>> reads;
    };
    Input input;
    struct Expectation{
        std::vector<ZoneStartMove> moves;
    };
    Expectation expectation;
};

const StartCase START_CASES[] = {
    {
        .name = "the hand-measured walk: 127 off the rocks, the marker walk to 102, then the search",
        .input = {.reads = {127, 102}},
        .expectation = {.moves = {WALK_TO_SPOT, SEARCH}},
    },
    {
        .name = "no read off the rocks: stop, no search at the rocks",
        .input = {.reads = {std::nullopt}},
        .expectation = {.moves = {ROCKS_NOT_LEFT}},
    },
    {
        .name = "a read off the rocks nearer than the save stands: stop",
        .input = {.reads = {113}},
        .expectation = {.moves = {ROCKS_NOT_LEFT}},
    },
    {
        .name = "a read off the rocks further than the save stands: stop",
        .input = {.reads = {140}},
        .expectation = {.moves = {ROCKS_NOT_LEFT}},
    },
    {
        .name = "the walk to 100 ends with no agreed read: stop",
        .input = {.reads = {127, std::nullopt}},
        .expectation = {.moves = {WALK_TO_SPOT, SPOT_NOT_REACHED}},
    },
    {
        .name = "the walk to 100 ends short of it: stop",
        .input = {.reads = {127, 115}},
        .expectation = {.moves = {WALK_TO_SPOT, SPOT_NOT_REACHED}},
    },
};

//  catch_in_zone()'s way to the spot: each read decides the next move.
UnitTestResult check_start(const StartCase& test){
    ZoneStart start(TAUNIE_ROCKS_ROUTE);
    std::vector<ZoneStartMove> moves;
    for (const std::optional<int>& read : test.input.reads){
        moves.push_back(start.next(read));
    }
    if (moves != test.expectation.moves){
        return std::format("expected moves {}; got moves {}", text(test.expectation.moves), text(moves));
    }
    return true;
}

class StartTest : public UnitTest{
public:
    explicit StartTest(StartCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - ZoneStart: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_start(m_case);
    }
private:
    StartCase m_case;
};

} // namespace


void add_tests_AutoStoryZoneSearch(UnitTestDatabase& database){
    for (const SearchCase& test : SEARCH_CASES){
        database.add<SearchTest>(test);
    }
    for (const StartCase& test : START_CASES){
        database.add<StartTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
