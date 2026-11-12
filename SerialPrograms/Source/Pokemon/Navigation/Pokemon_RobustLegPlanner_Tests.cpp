/*  Robust Leg Planner Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The robust planner on a small made-up game: walls hold the player, a
 *  door stops them without holding them, and open legs may end a tile off.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_RobustLegPlanner.h"
#include "Pokemon_RobustLegPlanner_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  '#' wall (and everything outside the rows), '.' floor, '"' tall grass,
//  'w' where someone wanders, 'D' a door: the player is stopped there but
//  not held, since walking on enters the building.
class ToyGame{
public:
    using State = TilePos;
    struct StateLess{
        bool operator()(const TilePos& a, const TilePos& b) const{
            return a.y != b.y ? a.y < b.y : a.x < b.x;
        }
    };

    explicit ToyGame(std::vector<std::string> rows)
        : m_rows(std::move(rows))
    {}

    std::optional<State> step(const State& state, Direction direction) const{
        const TilePos next = state.next(direction);
        const char tile = at(next);
        if (tile == '#' || tile == 'D'){
            return std::nullopt;
        }
        return next;
    }
    size_t step_tiles(const State&, const State&) const{
        return 1;
    }
    bool held_at(const State& state, Direction direction) const{
        return at(state.next(direction)) == '#';
    }
    bool wild(const State& state) const{
        return at(state) == '"';
    }
    bool wander(const State& state) const{
        return at(state) == 'w';
    }

private:
    char at(const TilePos& pos) const{
        if (pos.y < 0 || pos.y >= (int)m_rows.size() || pos.x < 0 || pos.x >= (int)m_rows[pos.y].size()){
            return '#';
        }
        return m_rows[pos.y][pos.x];
    }

    std::vector<std::string> m_rows;
};

using Legs = std::vector<RobustLeg<TilePos>>;

std::string describe(const std::optional<Legs>& legs){
    if (!legs){
        return " none";
    }
    std::string text;
    for (const RobustLeg<TilePos>& leg : *legs){
        text += std::format("\n  {} {}{} to", direction_name(leg.direction), leg.tiles, leg.to_wall ? " (to wall)" : " (open)");
        for (const TilePos& pos : leg.after){
            text += " " + to_string(pos);
        }
    }
    return text;
}

std::optional<Legs> plan(const ToyGame& game, std::vector<TilePos> from, TilePos to, const RobustLegCosts& costs){
    return plan_robust_legs(game, std::move(from), std::function<bool(const TilePos&)>([to](const TilePos& pos){ return pos == to; }), costs);
}

RobustLegCosts exact_costs(){
    RobustLegCosts costs;
    costs.exact_open_tiles = costs.max_open_tiles;
    return costs;
}


UnitTestResult test_wall_gathers_spread(){
    //  The player is on one of two tiles; walking east to the wall puts them
    //  on the goal either way.
    const ToyGame game({
        "...#",
    });
    const auto legs = plan(game, {{0, 0}, {1, 0}}, {2, 0}, RobustLegCosts());
    if (!legs || legs->size() != 1 || !(*legs)[0].to_wall || (*legs)[0].tiles != 2 || (*legs)[0].after != std::vector<TilePos>{{2, 0}}){
        return "Expected one leg east 2 to the wall:" + describe(legs);
    }
    return true;
}

UnitTestResult test_open_leg_spreads(){
    //  The goal is in the middle of a row with no wall near it: an open leg
    //  may end a tile either side, so there is no robust plan. With exact
    //  open legs, one open leg reaches it.
    const ToyGame game({
        "........",
    });
    const auto uncertain = plan(game, {{0, 0}}, {3, 0}, RobustLegCosts());
    if (uncertain){
        return "Expected no plan with uncertain open legs:" + describe(uncertain);
    }
    const auto exact = plan(game, {{0, 0}}, {3, 0}, exact_costs());
    if (!exact || exact->size() != 1 || (*exact)[0].to_wall || (*exact)[0].tiles != 3){
        return "Expected one open leg east 3:" + describe(exact);
    }
    return true;
}

UnitTestResult test_door_is_not_a_wall(){
    //  Walking east would stop at the door, but holding on enters it: the
    //  leg to the goal next to it must be an open one.
    const ToyGame game({
        "#..D",
    });
    const auto legs = plan(game, {{1, 0}}, {2, 0}, exact_costs());
    if (!legs || legs->size() != 1 || (*legs)[0].to_wall || (*legs)[0].tiles != 1){
        return "Expected one open leg east 1:" + describe(legs);
    }
    return true;
}

UnitTestResult test_avoids_wander_run(){
    //  The top row runs along where someone wanders; the bottom row is one
    //  longer but clear.
    const ToyGame game({
        ".www.",
        ".....",
    });
    const auto legs = plan(game, {{1, 0}}, {4, 0}, exact_costs());
    if (!legs){
        return "No plan.";
    }
    for (const RobustLeg<TilePos>& leg : *legs){
        if (leg.direction == Direction::EAST && leg.after[0].y == 0){
            return "Walked east along the wander zone:" + describe(legs);
        }
    }
    return true;
}

UnitTestResult test_cost_of_plan(){
    //  A wall leg costs a leg and its tiles; an open one its stop and open
    //  tiles on top.
    const ToyGame game({
        "...#",
    });
    RobustLegCosts costs = exact_costs();
    costs.per_open_tile = 2;
    const Legs wall{{Direction::EAST, 2, true, {{2, 0}}}};
    const Legs open{{Direction::EAST, 1, false, {{1, 0}}}};
    const long long wall_cost = robust_legs_cost(game, {{0, 0}}, wall, costs);
    const long long open_cost = robust_legs_cost(game, {{0, 0}}, open, costs);
    if (wall_cost != 8 + 2 || open_cost != 8 + 50 + 3){
        return std::format("Expected costs 10 and 61, got {} and {}.", wall_cost, open_cost);
    }
    return true;
}


struct RobustLegPlannerTestCase{
    std::string name;
    UnitTestResult (*test)();
};

class Test_RobustLegPlanner : public UnitTest{
public:
    explicit Test_RobustLegPlanner(RobustLegPlannerTestCase scenario)
        : UnitTest(std::format("Pokemon::RobustLegPlanner - {}", scenario.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.test();
    }

private:
    RobustLegPlannerTestCase m_scenario;
};

const RobustLegPlannerTestCase ROBUST_LEG_PLANNER_CASES[] = {
    {.name = "a wall gathers the possible tiles", .test = test_wall_gathers_spread},
    {.name = "an open leg may end a tile off", .test = test_open_leg_spreads},
    {.name = "a door is not a wall", .test = test_door_is_not_a_wall},
    {.name = "no leg along a wander zone", .test = test_avoids_wander_run},
    {.name = "cost of given legs", .test = test_cost_of_plan},
};

} // namespace


void add_tests_RobustLegPlanner(UnitTestDatabase& database){
    for (const auto& scenario : ROBUST_LEG_PLANNER_CASES){
        database.add<Test_RobustLegPlanner>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
