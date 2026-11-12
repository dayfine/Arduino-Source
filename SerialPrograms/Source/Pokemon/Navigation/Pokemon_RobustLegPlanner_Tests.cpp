/*  Robust Leg Planner Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The robust planner on a small made-up game: walls hold the player, a
 *  door stops them without holding them, a ledge hops them south, and open
 *  legs may end a tile off.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_RobustLegPlanner.h"
#include "Pokemon_RobustLegPlanner_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  '#' wall (and everything outside the rows), '.' floor, '"' tall grass,
//  'w' where someone wanders, 'D' a door: the player is stopped there but
//  not held, since walking on enters the building. 'v' a ledge: a step south
//  onto it hops to the tile past it and counts 2; it holds the player any
//  other way. Rows that wrap join their east end to their west end, so a
//  walk along them is never stopped.
class ToyGame{
public:
    using State = TilePos;
    using StateLess = std::less<TilePos>;

    ToyGame(const std::vector<std::string_view>& rows, bool wraps)
        : m_rows(rows.begin(), rows.end())
        , m_wraps(wraps)
    {}

    std::optional<State> step(const State& state, Direction direction) const{
        TilePos next = wrapped(state.next(direction));
        const char tile = at(next);
        if (tile == 'v' && direction == Direction::SOUTH){
            next = next.next(Direction::SOUTH);
            return at(next) == '#' ? std::nullopt : std::optional<State>(next);
        }
        if (tile == '#' || tile == 'D' || tile == 'v'){
            return std::nullopt;
        }
        return next;
    }
    size_t step_tiles(const State& from, const State& to) const{
        return to.y - from.y == 2 ? 2 : 1;
    }
    bool held_at(const State& state, Direction direction) const{
        return at(wrapped(state.next(direction))) != 'D';
    }
    bool wild(const State& state) const{
        return at(state) == '"';
    }
    bool wander(const State& state) const{
        return at(state) == 'w';
    }

private:
    TilePos wrapped(TilePos pos) const{
        if (m_wraps && pos.y >= 0 && pos.y < (int)m_rows.size()){
            const int width = (int)m_rows[pos.y].size();
            pos.x = (pos.x % width + width) % width;
        }
        return pos;
    }
    char at(const TilePos& pos) const{
        if (pos.y < 0 || pos.y >= (int)m_rows.size() || pos.x < 0 || pos.x >= (int)m_rows[pos.y].size()){
            return '#';
        }
        return m_rows[pos.y][pos.x];
    }

    std::vector<std::string> m_rows;
    bool m_wraps = false;
};

using Legs = std::vector<RobustLeg<TilePos>>;

//  Legs as "east 2 to wall: (2, 0); south 1 open: (2, 1) (2, 2)", or "none".
std::string describe(const std::optional<Legs>& legs){
    if (!legs){
        return "none";
    }
    std::string text;
    for (const RobustLeg<TilePos>& leg : *legs){
        text += std::format("{}{} {} {}:", text.empty() ? "" : "; ", direction_name(leg.direction), leg.tiles, leg.to_wall ? "to wall" : "open");
        for (const TilePos& pos : leg.after){
            text += " " + to_string(pos);
        }
    }
    return text;
}

constexpr RobustLegCosts EXACT{.exact_open_tiles = 12};


struct PlanInput{
    std::string_view name;
    std::vector<std::string_view> rows;
    bool wraps = false;
    std::vector<TilePos> from;
    TilePos goal;
    RobustLegCosts costs;
};
struct PlanExpectation{
    std::string_view legs;  //  as describe() writes them
};
struct PlanCase{
    PlanInput input;
    PlanExpectation expectation;
};

class Test_RobustLegPlanner_Plan : public UnitTest{
public:
    explicit Test_RobustLegPlanner_Plan(PlanCase scenario)
        : UnitTest(std::format("Pokemon::RobustLegPlanner - {}", scenario.input.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const PlanInput& input = m_scenario.input;
        const ToyGame game(input.rows, input.wraps);
        const TilePos goal = input.goal;
        const std::optional<Legs> legs = plan_robust_legs(game, input.from,
            std::function<bool(const TilePos&)>([goal](const TilePos& pos){ return pos == goal; }), input.costs);
        const std::string actual = describe(legs);
        if (actual != m_scenario.expectation.legs){
            return std::format("Planned \"{}\", expected \"{}\".", actual, m_scenario.expectation.legs);
        }
        return true;
    }
private:
    PlanCase m_scenario;
};

//  Open legs on default costs may end a tile either way (two up to 5 tiles).
const PlanCase PLAN_CASES[] = {
    {
        .input = {.name = "a wall gathers the possible tiles", .rows = {"...#"}, .from = {{0, 0}, {1, 0}}, .goal = {2, 0}},
        .expectation = {.legs = "east 2 to wall: (2, 0)"},
    },
    {
        .input = {.name = "no plan ends in the open while open legs may be off", .rows = {"........"}, .from = {{0, 0}}, .goal = {3, 0}},
        .expectation = {.legs = "none"},
    },
    {
        .input = {.name = "an exact open leg ends in the open", .rows = {"........"}, .from = {{0, 0}}, .goal = {3, 0}, .costs = EXACT},
        .expectation = {.legs = "east 3 open: (3, 0)"},
    },
    {
        .input = {
            .name = "open_error_per 0 with no open_error is exact", .rows = {"........"}, .from = {{0, 0}}, .goal = {3, 0},
            .costs = {.open_error = 0, .open_error_per = 0},
        },
        .expectation = {.legs = "east 3 open: (3, 0)"},
    },
    {
        .input = {
            .name = "open_error_per 0 keeps open_error", .rows = {"........"}, .from = {{0, 0}}, .goal = {3, 0},
            .costs = {.open_error = 1, .open_error_per = 0},
        },
        .expectation = {.legs = "none"},
    },
    {
        .input = {.name = "a door is not a wall", .rows = {"#..D"}, .from = {{1, 0}}, .goal = {2, 0}, .costs = EXACT},
        .expectation = {.legs = "east 1 open: (2, 0)"},
    },
    {
        .input = {.name = "no leg along a wander zone", .rows = {".www.", "....."}, .from = {{1, 0}}, .goal = {4, 0}, .costs = EXACT},
        .expectation = {.legs = "south 1 to wall: (1, 1); east 3 to wall: (4, 1); north 1 to wall: (4, 0)"},
    },
    {
        .input = {.name = "a ledge hop counts 2 tiles", .rows = {".", "v", "."}, .from = {{0, 0}}, .goal = {0, 2}},
        .expectation = {.legs = "south 2 to wall: (0, 2)"},
    },
    {
        .input = {.name = "a ledge cannot be climbed", .rows = {".", "v", "."}, .from = {{0, 2}}, .goal = {0, 0}},
        .expectation = {.legs = "none"},
    },
    {
        .input = {.name = "tall grass costs less than a detour", .rows = {".\"\"..", "....."}, .from = {{0, 0}}, .goal = {4, 0}},
        .expectation = {.legs = "east 4 to wall: (4, 0)"},
    },
    {
        .input = {
            .name = "costly tall grass is walked round", .rows = {".\"\"..", "....."}, .from = {{0, 0}}, .goal = {4, 0},
            .costs = {.per_wild_tile = 20},
        },
        .expectation = {.legs = "south 1 to wall: (0, 1); east 4 to wall: (4, 1); north 1 to wall: (4, 0)"},
    },
    {
        //  Only an open leg reaches the second row; a wall then gathers.
        .input = {.name = "an open leg spreads, a wall gathers", .rows = {".....", "#...#"}, .from = {{0, 0}}, .goal = {3, 1}},
        .expectation = {.legs = "east 1 open: (1, 0) (2, 0); south 1 to wall: (1, 1) (2, 1); east 2 to wall: (3, 1)"},
    },
    {
        .input = {
            .name = "a spread wider than max_spread is not planned", .rows = {".....", "#...#"}, .from = {{0, 0}}, .goal = {3, 1},
            .costs = {.max_spread = 1},
        },
        .expectation = {.legs = "none"},
    },
    {
        .input = {
            .name = "the search gives up past max_searched", .rows = {".....", "#...#"}, .from = {{0, 0}}, .goal = {3, 1},
            .costs = {.max_searched = 1},
        },
        .expectation = {.legs = "none"},
    },
    {
        .input = {
            .name = "two open legs the same way are one", .rows = {"........"}, .from = {{0, 0}}, .goal = {3, 0},
            .costs = {.exact_open_tiles = 12, .max_open_tiles = 2},
        },
        .expectation = {.legs = "east 3 open: (3, 0)"},
    },
    {
        .input = {.name = "a walk that is never stopped is not a wall", .rows = {"...."}, .wraps = true, .from = {{0, 0}}, .goal = {2, 0}, .costs = EXACT},
        .expectation = {.legs = "east 2 open: (2, 0)"},
    },
};


struct CostInput{
    std::string_view name;
    std::vector<std::string_view> rows;
    TilePos from;
    Legs legs;
    RobustLegCosts costs;
};
struct CostExpectation{
    long long cost = 0;
};
struct CostCase{
    CostInput input;
    CostExpectation expectation;
};

class Test_RobustLegPlanner_Cost : public UnitTest{
public:
    explicit Test_RobustLegPlanner_Cost(CostCase scenario)
        : UnitTest(std::format("Pokemon::RobustLegPlanner - {}", scenario.input.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const CostInput& input = m_scenario.input;
        const ToyGame game(input.rows, false);
        const long long actual = robust_legs_cost(game, {input.from}, input.legs, input.costs);
        if (actual != m_scenario.expectation.cost){
            return std::format("Cost {} for \"{}\", expected {}.", actual, describe(input.legs), m_scenario.expectation.cost);
        }
        return true;
    }
private:
    CostCase m_scenario;
};

//  Default costs: 8 a leg, 1 a tile, 4 more a grass tile, 50 more for a
//  leg that stops in the open.
const CostCase COST_CASES[] = {
    {
        .input = {.name = "a wall leg costs a leg and its tiles", .rows = {"...#"}, .from = {0, 0}, .legs = {{Direction::EAST, 2, true, {{2, 0}}}}},
        .expectation = {.cost = 8 + 2},
    },
    {
        .input = {
            .name = "an open leg costs its stop and open tiles on top", .rows = {"...#"}, .from = {0, 0},
            .legs = {{Direction::EAST, 1, false, {{1, 0}}}}, .costs = {.exact_open_tiles = 12, .per_open_tile = 2},
        },
        .expectation = {.cost = 8 + 50 + 3},
    },
    {
        .input = {.name = "each grass tile entered costs extra", .rows = {".\"\"#"}, .from = {0, 0}, .legs = {{Direction::EAST, 2, true, {{2, 0}}}}},
        .expectation = {.cost = 8 + 2 + 2 * 4},
    },
    {
        .input = {
            .name = "a cost with open_error_per 0", .rows = {"........"}, .from = {0, 0},
            .legs = {{Direction::EAST, 3, false, {{3, 0}}}}, .costs = {.open_error_per = 0},
        },
        .expectation = {.cost = 8 + 50 + 3},
    },
};

} // namespace


void add_tests_RobustLegPlanner(UnitTestDatabase& database){
    for (const PlanCase& scenario : PLAN_CASES){
        database.add<Test_RobustLegPlanner_Plan>(scenario);
    }
    for (const CostCase& scenario : COST_CASES){
        database.add<Test_RobustLegPlanner_Cost>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
