/*  Wall Recovery Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic diagnostics of the shared search. Game adapters retain the
 *  real forest/mine scenarios and collision/transition semantics.
 */

#include <array>
#include <format>
#include <string>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_WallRecovery.h"
#include "Pokemon_Navigation_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{
namespace{

struct TestHold{
    Direction direction;
    int tiles;
    bool operator==(const TestHold&) const = default;
};

//  A one-dimensional synthetic corridor, not an open map edge:
//      wall | walkable x=0 ... 10 | wall
//       -1  | . . A B . . . . . . . | 11
//  A/B are example hypotheses x=2/3. WEST stops at x=0; EAST at x=10.
//  NORTH/SOUTH are unavailable. A person anywhere on a ray makes it unsafe.
struct CorridorContext{
    std::optional<int> person;
};

class CorridorPolicy{
public:
    using Hypothesis = int;
    using Position = int;
    using Hold = TestHold;

    static Position position(Hypothesis at){ return at; }
    static int tiles(const Hold& hold){ return hold.tiles; }
    static std::optional<WallRecoveryStep<Hypothesis, Hold>> hold(
        const CorridorContext& context, const std::vector<Hypothesis>& from, Direction direction
    ){
        if (direction != Direction::WEST && direction != Direction::EAST){
            return std::nullopt;
        }
        const int end = direction == Direction::WEST ? 0 : 10;
        int longest = 0;
        for (int start : from){
            if (context.person && std::min(start, end) <= *context.person && *context.person <= std::max(start, end)){
                return std::nullopt;
            }
            longest = std::max(longest, start > end ? start - end : end - start);
        }
        return WallRecoveryStep<Hypothesis, Hold>{{end}, {direction, longest}};
    }

};

//  Show both directions and distances when a selected recovery plan differs.
std::string describe_holds(std::span<const TestHold> holds){
    std::string result;
    for (const auto& hold : holds){
        result += std::format("{} {} tiles; ", direction_name(hold.direction), hold.tiles);
    }
    return result.empty() ? "none" : result;
}

//  Inputs and expectations are separate aggregates; each registered test owns one row.
struct CorridorInput{
    std::string name;
    std::vector<int> positions;
    std::optional<int> person;
    size_t budget;
};

struct CorridorExpectation{
    std::optional<int> first_anchor;
    std::vector<TestHold> holds;
};

struct CorridorTestCase{
    CorridorInput input;
    CorridorExpectation expectation;
};

class Test_WallRecovery : public UnitTest{
public:
    explicit Test_WallRecovery(CorridorTestCase scenario)
        : UnitTest("Pokemon::WallRecovery - " + scenario.input.name), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const auto& input = m_scenario.input;
        const auto& expected = m_scenario.expectation;
        const auto found = find_wall_anchors<CorridorPolicy>(input.positions, CorridorContext{.person = input.person}, {.max_holds = input.budget});
        const std::optional<int> actual = found.empty() ? std::nullopt : std::optional<int>(found.front().at);
        if (actual != expected.first_anchor){
            return std::format("Expected first anchor {}, got {} ({} anchors).",
                expected.first_anchor ? std::to_string(*expected.first_anchor) : "none",
                actual ? std::to_string(*actual) : "none", found.size());
        }
        if (!found.empty() && found.front().holds != expected.holds){
            return std::format("Expected holds {}, got {}.", describe_holds(expected.holds), describe_holds(found.front().holds));
        }
        return true;
    }
private:
    CorridorTestCase m_scenario;
};

const CorridorTestCase CORRIDOR_CASES[] = {
    {
        .input = {.name = "empty", .positions = {}, .person = std::nullopt, .budget = 3},
        .expectation = {.first_anchor = std::nullopt, .holds = {}},
    },
    {
        .input = {.name = "already known", .positions = {4}, .person = std::nullopt, .budget = 0},
        .expectation = {.first_anchor = 4, .holds = {}},
    },
    {
        .input = {.name = "both tiles", .positions = {2, 3}, .person = std::nullopt, .budget = 3},
        .expectation = {.first_anchor = 0, .holds = {{.direction = Direction::WEST, .tiles = 3}}},
    },
    {
        .input = {.name = "three shortfall endpoints", .positions = {3, 4, 5}, .person = std::nullopt, .budget = 3},
        .expectation = {.first_anchor = 0, .holds = {{.direction = Direction::WEST, .tiles = 5}}},
    },
    {
        .input = {.name = "shortest hold first", .positions = {8, 9}, .person = std::nullopt, .budget = 3},
        .expectation = {.first_anchor = 10, .holds = {{.direction = Direction::EAST, .tiles = 2}}},
    },
    {
        .input = {.name = "person is not an anchor", .positions = {2, 3}, .person = 1, .budget = 3},
        .expectation = {.first_anchor = 10, .holds = {{.direction = Direction::EAST, .tiles = 8}}},
    },
    {
        .input = {.name = "person splits belief", .positions = {2, 8}, .person = 5, .budget = 3},
        .expectation = {.first_anchor = std::nullopt, .holds = {}},
    },
    {
        .input = {.name = "budget exhausted", .positions = {2, 3}, .person = std::nullopt, .budget = 0},
        .expectation = {.first_anchor = std::nullopt, .holds = {}},
    },
};

//  A synthetic closed room: all (x,y) in [0,10] x [0,10] are walkable.
//  Walls are outside that rectangle at x/y=-1 and x/y=11.
//      #############
//      #C..........#  y=0; C=(0,0), the expected northwest stop
//      #...........#
//      #...........#
//      #..A........#  A=(2,3)
//      #...B.......#  B=(3,4)
//      #...........#  ... through y=10
//      #############
//  NORTH 4 maps A/B to (2,0)/(3,0); WEST 3 then merges them at C.
//  Coordinates always stay inside the room; no unknown edge is crossed.
struct CornerContext{};

class CornerPolicy{
public:
    using Hypothesis = TilePos;
    using Position = TilePos;
    using Hold = TestHold;

    static Position position(const Hypothesis& at){ return at; }
    static int tiles(const Hold& hold){ return hold.tiles; }
    static std::optional<WallRecoveryStep<Hypothesis, Hold>> hold(
        const CornerContext&, const std::vector<Hypothesis>& from, Direction direction
    ){
        std::vector<Hypothesis> ends;
        int longest = 0;
        for (const TilePos& start : from){
            TilePos end = start;
            const bool vertical = direction == Direction::NORTH || direction == Direction::SOUTH;
            int& coordinate = vertical ? end.y : end.x;
            const int distance = direction == Direction::NORTH || direction == Direction::WEST ? coordinate : 10 - coordinate;
            coordinate = direction == Direction::NORTH || direction == Direction::WEST ? 0 : 10;
            longest = std::max(longest, distance);
            if (std::find(ends.begin(), ends.end(), end) == ends.end()){
                ends.push_back(end);
            }
        }
        return WallRecoveryStep<Hypothesis, Hold>{ends, {direction, longest}};
    }
};

struct CornerInput{
    std::string name;
    std::array<TilePos, 2> positions;
    size_t budget;
};

struct CornerExpectation{
    std::optional<TilePos> first_anchor;
    std::vector<TestHold> holds;
};

struct CornerTestCase{
    CornerInput input;
    CornerExpectation expectation;
};

class Test_WallRecoveryCorner : public UnitTest{
public:
    explicit Test_WallRecoveryCorner(CornerTestCase scenario)
        : UnitTest("Pokemon::WallRecovery - " + scenario.input.name), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const auto& input = m_scenario.input;
        const auto& expected = m_scenario.expectation;
        //  std::array and the corridor's std::vector both bind to the borrowed span API.
        const auto found = find_wall_anchors<CornerPolicy>(input.positions, CornerContext{}, {.max_holds = input.budget});
        const std::optional<TilePos> actual = found.empty() ? std::nullopt : std::optional<TilePos>(found.front().at);
        if (actual != expected.first_anchor){
            return std::format("Expected first corner {}, got {} ({} anchors).",
                expected.first_anchor ? to_string(*expected.first_anchor) : "none",
                actual ? to_string(*actual) : "none", found.size());
        }
        if (!found.empty() && found.front().holds != expected.holds){
            return std::format("Expected holds {}, got {}.",
                describe_holds(expected.holds), describe_holds(found.front().holds));
        }
        return true;
    }
private:
    CornerTestCase m_scenario;
};

const CornerTestCase CORNER_CASES[] = {
    {
        .input = {.name = "one hold cannot localize both axes", .positions = {{{.x = 2, .y = 3}, {.x = 3, .y = 4}}}, .budget = 1},
        .expectation = {.first_anchor = std::nullopt, .holds = {}},
    },
    {
        .input = {.name = "two holds converge at northwest corner", .positions = {{{.x = 2, .y = 3}, {.x = 3, .y = 4}}}, .budget = 2},
        .expectation = {.first_anchor = TilePos{.x = 0, .y = 0},
            .holds = {{.direction = Direction::NORTH, .tiles = 4}, {.direction = Direction::WEST, .tiles = 3}}},
    },
};

} // namespace

void add_tests_WallRecovery(UnitTestDatabase& database){
    for (const CorridorTestCase& scenario : CORRIDOR_CASES){
        database.add<Test_WallRecovery>(scenario);
    }
    for (const CornerTestCase& scenario : CORNER_CASES){
        database.add<Test_WallRecoveryCorner>(scenario);
    }
}

} // namespace Pokemon
} // namespace PokemonAutomation
