/*  PokemonLZA AutoStory - Wild Zone 1 road Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  03.08's way from Taunie's rocks onto the road (ZoneStart) and its walks
 *  between the road stops (RoadWalkReads): each case a name, the distance
 *  reads in order, and the moves they lead to.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryZone1Road.h"
#include "PokemonLZA_AutoStoryZoneStart.h"
#include "PokemonLZA_AutoStoryZone1Road_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr std::optional<int> NONE = std::nullopt;

std::string_view name_of(ZoneStartMove move){
    switch (move){
    case ZoneStartMove::WALK_TO_SPOT:
        return "WALK_TO_SPOT";
    case ZoneStartMove::SEARCH:
        return "SEARCH";
    case ZoneStartMove::ROCKS_NOT_LEFT:
        return "ROCKS_NOT_LEFT";
    case ZoneStartMove::SPOT_NOT_REACHED:
        return "SPOT_NOT_REACHED";
    }
    return "?";
}

std::string_view name_of(RoadLook look){
    switch (look){
    case RoadLook::STEP:
        return "STEP";
    case RoadLook::SIDESTEP_RIGHT:
        return "SIDESTEP_RIGHT";
    case RoadLook::SIDESTEP_LEFT:
        return "SIDESTEP_LEFT";
    case RoadLook::ARRIVED:
        return "ARRIVED";
    }
    return "?";
}

template <typename Move>
std::string text(const std::vector<Move>& moves){
    std::string out;
    for (const Move& move : moves){
        out += out.empty() ? std::string(name_of(move)) : std::format(", {}", name_of(move));
    }
    return out;
}

std::string text(std::optional<int> distance){
    return distance ? std::to_string(*distance) : std::string("none");
}

template <typename Move>
std::vector<Move> repeated(Move move, size_t times){
    return std::vector<Move>(times, move);
}

template <typename Type>
std::vector<Type> operator+(std::vector<Type> front, const std::vector<Type>& back){
    front.insert(front.end(), back.begin(), back.end());
    return front;
}


//  ZoneStart on TAUNIE_ROCKS_ROUTE: the leg off the rocks, its read, the
//  timed walk toward the objective (walk_hold), its read.
struct StartCase{
    std::string name;
    struct Input{
        //  The pin read at the start, before the leg (none: not read).
        std::optional<MapPoint> start_pin;
        //  The read after the leg off the rocks, then after the timed walk
        //  (none: no read taken).
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
        .name = "the hand-measured way: 127 off the rocks, the timed walk to 102, then the patrol",
        .input = {.reads = {127, 102}},
        .expectation = {.moves = {ZoneStartMove::WALK_TO_SPOT, ZoneStartMove::SEARCH}},
    },
    {
        //  console B, 2026-11-16 16:14: 101 on screen, the digits over the
        //  green column read "1", "41" and "5".
        .name = "no read after the timed walk: the patrol, which reads on its walk to 100",
        .input = {.reads = {127, NONE}},
        .expectation = {.moves = {ZoneStartMove::WALK_TO_SPOT, ZoneStartMove::SEARCH}},
    },
    {
        .name = "the timed walk ends short of the spot: stop",
        .input = {.reads = {127, 115}},
        .expectation = {.moves = {ZoneStartMove::WALK_TO_SPOT, ZoneStartMove::SPOT_NOT_REACHED}},
    },
    {
        .name = "no read off the rocks: stop, no walk",
        .input = {.reads = {NONE}},
        .expectation = {.moves = {ZoneStartMove::ROCKS_NOT_LEFT}},
    },
    {
        .name = "a read off the rocks nearer than the save stands: stop",
        .input = {.reads = {113}},
        .expectation = {.moves = {ZoneStartMove::ROCKS_NOT_LEFT}},
    },
    {
        .name = "a read off the rocks further than the save stands: stop",
        .input = {.reads = {140}},
        .expectation = {.moves = {ZoneStartMove::ROCKS_NOT_LEFT}},
    },
    {
        //  console B, French, 2026-11-17 02:41: the pin read as in every
        //  passing run, the marker's digits in none of 24 looks.
        .name = "the pin at the rocks, no read off them: the walk",
        .input = {.start_pin = MapPoint{.x = 476, .y = 224}, .reads = {NONE}},
        .expectation = {.moves = {ZoneStartMove::WALK_TO_SPOT}},
    },
    {
        .name = "the pin a step off the rocks, a read out of the leg's range: the walk",
        .input = {.start_pin = MapPoint{.x = 471, .y = 227}, .reads = {113}},
        .expectation = {.moves = {ZoneStartMove::WALK_TO_SPOT}},
    },
    {
        .name = "the pin at the 100 spot, a read of 127: stop",
        .input = {.start_pin = MapPoint{.x = 432, .y = 238}, .reads = {127}},
        .expectation = {.moves = {ZoneStartMove::ROCKS_NOT_LEFT}},
    },
};

class Test_ZoneStart : public UnitTest{
public:
    explicit Test_ZoneStart(const StartCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - ZoneStart: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ZoneStart start(TAUNIE_ROCKS_ROUTE);
        std::vector<ZoneStartMove> moves;
        for (size_t i = 0; i < m_case.input.reads.size(); i++){
            const std::optional<int> read = m_case.input.reads[i];
            moves.push_back(i == 0 ? start.leave(m_case.input.start_pin, read) : start.next(read));
        }
        if (moves != m_case.expectation.moves){
            return std::format("Expected [{}], got [{}].", text(m_case.expectation.moves), text(moves));
        }
        return true;
    }

private:
    const StartCase& m_case;
};


//  RoadWalkReads: a walk between two road stops, look by look.
struct RoadWalkCase{
    std::string name;
    struct Input{
        MarkerDirection direction;
        int stop_at;
        std::optional<int> floor;
        //  Each look's read (none: no digits).
        std::vector<std::optional<int>> reads;
    };
    Input input;
    struct Expectation{
        std::vector<RoadLook> moves;
        std::optional<int> position;
    };
    Expectation expectation;
};

//  The patrol's floor on walks toward the objective: 60 less its slack.
constexpr int FLOOR = CATCH_SPOTS[1] - ROAD_STOP_SLACK;

const RoadWalkCase ROAD_WALK_CASES[] = {
    {
        //  console B, 2026-11-16 16:17-16:21: the lean walker stood on
        //  looks like these and ended at no read, 74 and 92.
        .name = "100 to 60: on through looks with no read or misreads, arriving at 59",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 60, .floor = FLOOR, .reads = {
            98, NONE, NONE, 41, 92, NONE, 5, 86, 80, NONE, NONE, NONE, 72, 66, 63, 59,
        }},
        .expectation = {
            .moves = repeated(RoadLook::STEP, 15) + std::vector<RoadLook>{RoadLook::ARRIVED},
            .position = 59,
        },
    },
    {
        //  console B, 2026-11-16: 101 to 105 by the green column read "1",
        //  "41", "5" and "21".
        .name = "to 100 from the column: the misreads below the floor are not taken, the first true read is",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 100, .floor = FLOOR, .reads = {1, 41, 5, 21, 103, 99}},
        .expectation = {
            .moves = repeated(RoadLook::STEP, 5) + std::vector<RoadLook>{RoadLook::ARRIVED},
            .position = 99,
        },
    },
    {
        .name = "a read below the floor near the last one taken is taken",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 50, .floor = FLOOR, .reads = {62, 56}},
        .expectation = {.moves = {RoadLook::STEP, RoadLook::STEP}, .position = 56},
    },
    {
        //  console B, 2026-11-13: 110 read as "1110".
        .name = "three agreeing reads far from a wrong first read replace it",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 60, .floor = FLOOR, .reads = {1110, 108, 106, 104}},
        .expectation = {.moves = repeated(RoadLook::STEP, 4), .position = 104},
    },
    {
        .name = "60 back to 100: one far misread does not end the walk",
        .input = {.direction = MarkerDirection::BACK, .stop_at = 100, .floor = NONE, .reads = {62, 140, 66, NONE, 70, 76, 84, 92, 99, 101}},
        .expectation = {
            .moves = repeated(RoadLook::STEP, 9) + std::vector<RoadLook>{RoadLook::ARRIVED},
            .position = 101,
        },
    },
    {
        //  The Research Lab's front fence (console B, 2026-11-10).
        .name = "toward, held up: a sidestep right after 4 reads none nearer, then left",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 60, .floor = FLOOR, .reads = {80, 80, 81, 80, 80, 80, 80, 80, 80}},
        .expectation = {
            .moves = repeated(RoadLook::STEP, 4) + std::vector<RoadLook>{RoadLook::SIDESTEP_RIGHT}
                + repeated(RoadLook::STEP, 3) + std::vector<RoadLook>{RoadLook::SIDESTEP_LEFT},
            .position = 80,
        },
    },
    {
        .name = "back, standing at 65: no sidestep, on back",
        .input = {.direction = MarkerDirection::BACK, .stop_at = 100, .floor = NONE, .reads = {65, 65, 65, 65, 65, 65}},
        .expectation = {.moves = repeated(RoadLook::STEP, 6), .position = 65},
    },
};

class Test_RoadWalkReads : public UnitTest{
public:
    explicit Test_RoadWalkReads(const RoadWalkCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - RoadWalkReads: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        RoadWalkReads reads(m_case.input.direction, m_case.input.stop_at, m_case.input.floor);
        std::vector<RoadLook> moves;
        for (const std::optional<int>& read : m_case.input.reads){
            moves.push_back(reads.next(read));
        }
        if (moves != m_case.expectation.moves){
            return std::format("Expected [{}], got [{}].", text(m_case.expectation.moves), text(moves));
        }
        if (reads.position() != m_case.expectation.position){
            return std::format("Expected the walk at {}, got {}.", text(m_case.expectation.position), text(reads.position()));
        }
        return true;
    }

private:
    const RoadWalkCase& m_case;
};


} // namespace


void add_tests_AutoStoryZone1Road(UnitTestDatabase& database){
    for (const StartCase& item : START_CASES){
        database.add<Test_ZoneStart>(item);
    }
    for (const RoadWalkCase& item : ROAD_WALK_CASES){
        database.add<Test_RoadWalkReads>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
