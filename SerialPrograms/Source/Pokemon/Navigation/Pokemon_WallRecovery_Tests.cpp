/*  Wall Recovery Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic diagnostics of the shared search. Game adapters retain the
 *  real forest/mine scenarios and collision/transition semantics.
 */

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
};

class CorridorModel{
public:
    using Hypothesis = int;
    using Position = int;
    using Hold = TestHold;

    explicit CorridorModel(std::optional<int> person = std::nullopt)
        : m_person(person)
    {}
    Position position(Hypothesis at) const{ return at; }
    int tiles(const Hold& hold) const{ return hold.tiles; }
    std::optional<WallRecoveryStep<Hypothesis, Hold>> hold(
        const std::vector<Hypothesis>& from, Direction direction
    ) const{
        if (direction != Direction::WEST && direction != Direction::EAST){
            return std::nullopt;
        }
        const int end = direction == Direction::WEST ? 0 : 10;
        int longest = 0;
        for (int start : from){
            if (m_person && std::min(start, end) <= *m_person && *m_person <= std::max(start, end)){
                return std::nullopt;
            }
            longest = std::max(longest, start > end ? start - end : end - start);
        }
        return WallRecoveryStep<Hypothesis, Hold>{{end}, {direction, longest}};
    }

private:
    std::optional<int> m_person;
};

class CornerModel{
public:
    using Hypothesis = TilePos;
    using Position = TilePos;
    using Hold = TestHold;

    Position position(const Hypothesis& at) const{ return at; }
    int tiles(const Hold& hold) const{ return hold.tiles; }
    std::optional<WallRecoveryStep<Hypothesis, Hold>> hold(
        const std::vector<Hypothesis>& from, Direction direction
    ) const{
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

class Test_WallRecoveryCorner : public UnitTest{
public:
    Test_WallRecoveryCorner() : UnitTest("Pokemon::WallRecovery - two holds and depth budget"){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<TilePos> from{{2, 3}, {3, 4}};
        if (!find_wall_anchors(CornerModel(), from, {.max_holds = 1}).empty()){
            return "One hold cannot localize both axes.";
        }
        const auto found = find_wall_anchors(CornerModel(), from, {.max_holds = 2});
        if (found.empty() || found[0].holds.size() != 2 || found[0].at != TilePos{0, 0}){
            return "Expected two shortest holds to the northwest corner.";
        }
        return true;
    }
};

class Test_WallRecovery : public UnitTest{
public:
    Test_WallRecovery(std::string name, std::vector<int> from, std::optional<int> person,
        size_t budget, std::optional<int> expected, size_t holds)
        : UnitTest("Pokemon::WallRecovery - " + name)
        , m_from(std::move(from)), m_person(person), m_budget(budget), m_expected(expected), m_holds(holds)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const auto found = find_wall_anchors(CorridorModel(m_person), m_from, {.max_holds = m_budget});
        if (!m_expected){
            return found.empty() ? UnitTestResult(true) : UnitTestResult("Expected no safe anchor.");
        }
        if (found.empty() || found[0].at != *m_expected || found[0].holds.size() != m_holds){
            return "Wrong convergence tile or hold count.";
        }
        return true;
    }

private:
    std::vector<int> m_from;
    std::optional<int> m_person;
    size_t m_budget;
    std::optional<int> m_expected;
    size_t m_holds;
};

}

void add_tests_WallRecovery(UnitTestDatabase& database){
    database.add<Test_WallRecoveryCorner>();
    database.add<Test_WallRecovery>("empty", std::vector<int>{}, std::nullopt, 3, std::nullopt, 0);
    database.add<Test_WallRecovery>("already known", std::vector<int>{4}, std::nullopt, 0, 4, 0);
    database.add<Test_WallRecovery>("both tiles", std::vector<int>{2, 3}, std::nullopt, 3, 0, 1);
    database.add<Test_WallRecovery>("three shortfall endpoints", std::vector<int>{3, 4, 5}, std::nullopt, 3, 0, 1);
    database.add<Test_WallRecovery>("shortest hold first", std::vector<int>{8, 9}, std::nullopt, 3, 10, 1);
    database.add<Test_WallRecovery>("person is not an anchor", std::vector<int>{2, 3}, 1, 3, 10, 1);
    database.add<Test_WallRecovery>("person splits belief", std::vector<int>{2, 8}, 5, 3, std::nullopt, 0);
    database.add<Test_WallRecovery>("budget exhausted", std::vector<int>{2, 3}, std::nullopt, 0, std::nullopt, 0);
}

}
}
