/*  Leg Planner Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The planner on a small made-up game, with the kinds of tiles the real
 *  games have: walls, people, tall grass, a one-way tile and a warp.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_LegPlanner.h"
#include "Pokemon_Navigation_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  '#' wall, '.' floor, '"' tall grass, 'p' a person (blocks, but may walk
//  off, so not a wall to stop against), 'v' entered only going south, 'W'
//  a warp to the tile marked 'X'.
class ToyGame{
public:
    using State = TilePos;
    using StateHash = std::hash<TilePos>;

    explicit ToyGame(std::vector<std::string> rows)
        : m_rows(std::move(rows))
    {
        for (int y = 0; y < (int)m_rows.size(); y++){
            for (int x = 0; x < (int)m_rows[y].size(); x++){
                if (m_rows[y][x] == 'X'){
                    m_warp_to = {x, y};
                }
            }
        }
    }

    std::optional<Step<State>> press(const State& state, Direction direction) const{
        const TilePos next = state.next(direction);
        const char tile = at(next);
        if (tile == '#' || tile == 'p' || (tile == 'v' && direction != Direction::SOUTH)){
            return std::nullopt;
        }
        Step<State> step;
        step.next = next;
        step.facing = direction;
        if (tile == '"'){
            step.extra_cost = 5;
            step.wild = true;
        }
        if (tile == 'W'){
            step.next = m_warp_to;
            step.warped = true;
            step.ends_leg = true;
            step.extra_cost = 10;
        }
        return step;
    }
    bool held_at(const State& state, Direction direction) const{
        const char tile = at(state.next(direction));
        return tile == '#' || (tile == 'v' && direction != Direction::SOUTH);
    }

private:
    char at(const TilePos& pos) const{
        if (pos.y < 0 || pos.y >= (int)m_rows.size() || pos.x < 0 || pos.x >= (int)m_rows[pos.y].size()){
            return '#';
        }
        return m_rows[pos.y][pos.x];
    }

    std::vector<std::string> m_rows;
    TilePos m_warp_to;
};


std::string describe(const std::vector<Leg<TilePos>>& legs){
    std::string text;
    for (const Leg<TilePos>& leg : legs){
        text += "\n  ";
        text += leg.kind == LegKind::FACE ? "face " : "walk ";
        text += direction_name(leg.direction);
        if (leg.kind == LegKind::WALK){
            text += std::format(" {}", leg.tiles);
        }
        if (leg.to_wall){
            text += " (to wall)";
        }
        if (leg.wild){
            text += " (wild)";
        }
        if (leg.warps){
            text += " (warp)";
        }
        text += " -> " + to_string(leg.end);
    }
    return text;
}

std::optional<std::vector<Leg<TilePos>>> plan(
    const ToyGame& game, TilePos from, TilePos to,
    std::optional<Direction> start_facing = std::nullopt,
    std::optional<Direction> goal_facing = std::nullopt,
    const LegCosts& costs = LegCosts()
){
    return plan_legs(game, from, start_facing, std::function<bool(const TilePos&)>([to](const TilePos& pos){ return pos == to; }), goal_facing, costs);
}

//  The open-stop weight after a wild step, high enough to outweigh any detour
//  in these grids.
LegCosts wild_stop_costs(){
    LegCosts costs;
    costs.per_open_wild_stop = 300;
    return costs;
}


UnitTestResult test_prefers_wall_legs(){
    //  Two ways of equal length: east then south ends both legs in the open;
    //  south then east ends the last one against the pillar.
    const ToyGame game({
        "......",
        "...#..",
        "......",
    });
    const auto legs = plan(game, {0, 0}, {2, 1});
    if (!legs || legs->size() != 2){
        return "Expected 2 legs:" + (legs ? describe(*legs) : std::string(" none"));
    }
    if ((*legs)[1].direction != Direction::EAST || !(*legs)[1].to_wall){
        return "Expected to end going east into the pillar:" + describe(*legs);
    }
    return true;
}

UnitTestResult test_person_is_not_a_wall(){
    //  The only way ends next to a person, which isn't a wall to stop against.
    const ToyGame game({
        "#####",
        "#..p#",
        "#####",
    });
    const auto legs = plan(game, {1, 1}, {2, 1});
    if (!legs || legs->size() != 1 || (*legs)[0].to_wall){
        return "Expected one open leg:" + (legs ? describe(*legs) : std::string(" none"));
    }
    return true;
}

UnitTestResult test_avoids_grass(){
    //  Straight through the grass is 4 tiles; around it is 8, cheaper with
    //  5 extra per grass tile.
    const ToyGame game({
        ".....",
        ".\"\"\".",
        ".....",
    });
    const auto legs = plan(game, {0, 1}, {4, 1});
    if (!legs){
        return "No route.";
    }
    for (const Leg<TilePos>& leg : *legs){
        if (leg.end.y == 1 && leg.end.x > 0 && leg.end.x < 4){
            return "Walked into the grass:" + describe(*legs);
        }
    }
    return true;
}

UnitTestResult test_one_way_tile(){
    //  The 'v' can only be entered going south, so going back up must go around.
    const ToyGame game({
        "#.#.#",
        "#v#.#",
        "#...#",
        "#####",
    });
    if (!plan(game, {1, 0}, {1, 2})){
        return "No route down through the one-way tile.";
    }
    const auto up = plan(game, {1, 2}, {1, 0});
    if (up){
        return "Went up through the one-way tile:" + describe(*up);
    }
    return true;
}

UnitTestResult test_warp_ends_leg(){
    //  The warp on the left takes the player to X on the right.
    const ToyGame game({
        "#######",
        "#W.#X.#",
        "#..#..#",
        "#######",
    });
    const auto legs = plan(game, {2, 2}, {5, 2});
    if (!legs){
        return "No route.";
    }
    bool warped = false;
    for (const Leg<TilePos>& leg : *legs){
        if (leg.warps){
            warped = true;
            if (!(leg.end == TilePos{4, 1})){
                return std::format("The warp leg ends at {}:{}", to_string(leg.end), describe(*legs));
            }
        }
    }
    if (!warped){
        return "Expected a warp leg:" + describe(*legs);
    }
    return true;
}

UnitTestResult test_goal_facing(){
    const ToyGame game({
        "....",
    });
    const auto legs = plan(game, {0, 0}, {2, 0}, std::nullopt, Direction::NORTH);
    if (!legs || legs->back().kind != LegKind::FACE || legs->back().direction != Direction::NORTH){
        return "Expected to end facing north:" + (legs ? describe(*legs) : std::string(" none"));
    }
    return true;
}

UnitTestResult test_wild_leg(){
    //  The only way crosses the grass: that leg is wild. One that doesn't isn't.
    const ToyGame grass({
        ".\"\".",
    });
    const auto through = plan(grass, {0, 0}, {3, 0});
    if (!through || through->size() != 1 || !(*through)[0].wild){
        return "Expected one wild leg:" + (through ? describe(*through) : std::string(" none"));
    }
    const ToyGame floor({
        "....",
    });
    const auto across = plan(floor, {0, 0}, {3, 0});
    if (!across || across->size() != 1 || (*across)[0].wild){
        return "Expected one leg that isn't wild:" + (across ? describe(*across) : std::string(" none"));
    }
    return true;
}

UnitTestResult test_open_wild_stop(){
    //  Two ways down to the goal at (3, 2): turn south right after the
    //  grass, stopping in the open, or walk on to the wall and come back
    //  along the bottom row. The first is shorter by more than an open stop
    //  costs, but its open stop follows grass, so with the open-wild-stop
    //  weight the second wins.
    const ToyGame game({
        ".\"\"..........",
        "###.########.",
        "###..........",
    });
    const auto plain = plan(game, {0, 0}, {3, 2});
    if (!plain || plain->empty() || (*plain)[0].to_wall || !(*plain)[0].wild || (*plain)[0].end.x != 3){
        return "Without the weight, expected a wild leg east to stop in the open at x = 3:" + (plain ? describe(*plain) : std::string(" none"));
    }
    const auto weighed = plan(game, {0, 0}, {3, 2}, std::nullopt, std::nullopt, wild_stop_costs());
    if (!weighed || weighed->empty() || !(*weighed)[0].to_wall){
        return "With the weight, expected to walk east to the wall:" + (weighed ? describe(*weighed) : std::string(" none"));
    }
    return true;
}

UnitTestResult test_open_stop_without_grass(){
    //  The same map without grass: the weight changes nothing.
    const ToyGame game({
        ".............",
        "###.########.",
        "###..........",
    });
    const auto legs = plan(game, {0, 0}, {3, 2}, std::nullopt, std::nullopt, wild_stop_costs());
    if (!legs || legs->empty() || (*legs)[0].to_wall || (*legs)[0].wild || (*legs)[0].end.x != 3){
        return "Expected to stop in the open at x = 3:" + (legs ? describe(*legs) : std::string(" none"));
    }
    return true;
}

UnitTestResult test_wild_ends_with_leg(){
    //  East through the grass to the wall, then south. The south leg has no
    //  wild step, so it still stops in the open at (2, 2) to turn east to
    //  the goal at (4, 2), rather than going around by the bottom row.
    const ToyGame game({
        ".\"\"###",
        "##.###",
        "##...#",
        "##.#.#",
        "##.#.#",
        "##.#.#",
        "##.#.#",
        "##.#.#",
        "##.#.#",
        "##.#.#",
        "##.#.#",
        "##.#.#",
        "##...#",
    });
    const auto legs = plan(game, {0, 0}, {4, 2}, std::nullopt, std::nullopt, wild_stop_costs());
    if (!legs || legs->size() != 3){
        return "Expected 3 legs:" + (legs ? describe(*legs) : std::string(" none"));
    }
    const Leg<TilePos>& east = (*legs)[0];
    const Leg<TilePos>& south = (*legs)[1];
    if (!east.wild || !east.to_wall){
        return "Expected a wild leg east to the wall:" + describe(*legs);
    }
    if (south.wild || south.to_wall || !(south.end == TilePos{2, 2})){
        return "Expected the south leg to stop in the open at (2, 2):" + describe(*legs);
    }
    return true;
}

UnitTestResult test_unreachable(){
    const ToyGame game({
        ".#.",
    });
    if (plan(game, {0, 0}, {2, 0})){
        return "Planned through a wall.";
    }
    return true;
}


class Test_LegPlanner : public UnitTest{
public:
    Test_LegPlanner(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("Pokemon::LegPlanner - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

}


void add_tests_LegPlanner(UnitTestDatabase& database){
    database.add<Test_LegPlanner>("prefers legs that end at a wall", test_prefers_wall_legs);
    database.add<Test_LegPlanner>("a person is not a wall", test_person_is_not_a_wall);
    database.add<Test_LegPlanner>("avoids tall grass", test_avoids_grass);
    database.add<Test_LegPlanner>("one-way tile", test_one_way_tile);
    database.add<Test_LegPlanner>("a warp ends the leg", test_warp_ends_leg);
    database.add<Test_LegPlanner>("turns to face the goal", test_goal_facing);
    database.add<Test_LegPlanner>("nothing through a wall", test_unreachable);
    database.add<Test_LegPlanner>("a leg through grass is wild", test_wild_leg);
    database.add<Test_LegPlanner>("an open stop after grass is weighed", test_open_wild_stop);
    database.add<Test_LegPlanner>("the weight needs a wild step", test_open_stop_without_grass);
    database.add<Test_LegPlanner>("a new leg is not wild", test_wild_ends_with_leg);
}



}
}
