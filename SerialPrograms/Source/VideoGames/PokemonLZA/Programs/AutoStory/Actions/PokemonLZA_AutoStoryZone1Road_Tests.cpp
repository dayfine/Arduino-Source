/*  PokemonLZA AutoStory - Wild Zone 1 road Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  03.08's way from Taunie's rocks onto the road (ZoneStart): each case a
 *  name, the distance reads in order, and the moves they lead to.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
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

template <typename Move>
std::string text(const std::vector<Move>& moves){
    std::string out;
    for (const Move& move : moves){
        out += out.empty() ? std::string(name_of(move)) : std::format(", {}", name_of(move));
    }
    return out;
}



struct StartCase{
    std::string name;
    struct Input{
        //  None: not read.
        std::optional<MapPoint> start_pin;
        //  After the leg, then after the timed walk (none: no read taken).
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


} // namespace


void add_tests_AutoStoryZone1Road(UnitTestDatabase& database){
    for (const StartCase& item : START_CASES){
        database.add<Test_ZoneStart>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
