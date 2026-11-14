/*  AutoStory Boulders Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Victory Road's boulder puzzles: a search over where the boulders stand
 *  (Pokemon_BoulderSolver below), and the sim's PUSH_BOULDER, which the
 *  search's answer is replayed on.
 *
 *  TODO(map edit, needs the user's OK): the Victory Road maps have the
 *  boulders (objects named "pushable_boulder") but not the switches'
 *  barriers or which tiles the switches press, so the planner walks
 *  through the barriers. The data below is from the decompilation
 *  (pokefirered data/maps/VictoryRoad_*), kept here until the maps carry
 *  it as `gates` and `switches`; then this search reads them from the map.
 *
 */

#include <algorithm>
#include <deque>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldTiles.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows19_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::MapData;
using Pokemon::MapObject;
using Pokemon::TilePos;
using Pokemon::Direction;

namespace{

struct AutoStoryBouldersInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryBouldersExpectation{
    std::string_view description;
};

struct AutoStoryBouldersCase{
    AutoStoryBouldersInput input;
    AutoStoryBouldersExpectation expectation;
};

class Test_AutoStoryBoulders : public UnitTest{
public:
    explicit Test_AutoStoryBoulders(AutoStoryBouldersCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Boulders: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStoryBouldersCase m_case;
};


//  One floor's puzzle: where the player enters, the tiles that stay shut
//  until a switch is pressed, and the tiles a boulder presses.
struct BoulderPuzzle{
    std::string map;
    TilePos player;
    std::vector<TilePos> barriers;
    std::vector<TilePos> switches;
};

//  Victory Road 1F: the barriers at (12, 14) and (12, 15), opened by the
//  switch at (20, 16) (the map's B tile).
BoulderPuzzle victory_road_1f(){
    return {"VictoryRoad_1F", {11, 19}, {{12, 14}, {12, 15}}, {{20, 16}}};
}

const Direction DIRECTIONS[] = {Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST};

//  The player's moves (a step each; one onto a boulder pushes it) that put
//  a boulder on a switch, or nothing if no moves do. Breadth first over
//  the player's tile and the boulders': the fewest moves.
std::optional<std::vector<Direction>> solve_boulders(const MapData& map, const BoulderPuzzle& puzzle){
    std::vector<TilePos> boulders;
    for (const MapObject& object : map.objects){
        if (object.name == "pushable_boulder"){
            boulders.push_back({object.x, object.y});
        }
    }
    const std::set<TilePos> barriers(puzzle.barriers.begin(), puzzle.barriers.end());
    const std::set<TilePos> switches(puzzle.switches.begin(), puzzle.switches.end());
    //  Other people stand where they stand.
    std::set<TilePos> people;
    for (const MapObject& object : map.objects){
        if (object.name != "pushable_boulder"){
            people.insert({object.x, object.y});
        }
    }
    const auto floor = [&](const TilePos& tile){
        return switches.count(tile) != 0 || (!is_wall(map, tile) && barriers.count(tile) == 0 && people.count(tile) == 0);
    };
    using State = std::pair<TilePos, std::vector<TilePos>>;
    std::set<State> seen;
    std::deque<std::pair<State, std::vector<Direction>>> queue;
    State start{puzzle.player, boulders};
    seen.insert(start);
    queue.push_back({start, {}});
    while (!queue.empty() && seen.size() < 2000000){
        auto [state, moves] = std::move(queue.front());
        queue.pop_front();
        for (Direction direction : DIRECTIONS){
            State next = state;
            const TilePos ahead = state.first.next(direction);
            next.first = ahead;
            const auto boulder = std::find(next.second.begin(), next.second.end(), ahead);
            if (boulder != next.second.end()){
                const TilePos beyond = ahead.next(direction);
                if (!floor(beyond) || std::find(next.second.begin(), next.second.end(), beyond) != next.second.end()){
                    continue;
                }
                *boulder = beyond;
            }else if (!floor(ahead)){
                continue;
            }
            if (!seen.insert(next).second){
                continue;
            }
            std::vector<Direction> more = moves;
            more.push_back(direction);
            const bool pressed = std::any_of(next.second.begin(), next.second.end(), [&](const TilePos& tile){
                return switches.count(tile) != 0;
            });
            if (pressed){
                return more;
            }
            queue.push_back({std::move(next), std::move(more)});
        }
    }
    return std::nullopt;
}

const MapData& map_named(const std::string& name){
    for (const MapData& map : story_maps()){
        if (map.name == name){
            return map;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No map {}.", name));
}

//  A boulder one tile north of the player moves one tile north, and the
//  step after takes the player onto its old tile.
UnitTestResult test_sim_pushes_a_boulder(){
    OverworldSim sim(story_maps(), MapPosition{"VictoryRoad_1F", {7, 19}}, Direction::NORTH);
    if (!sim.use_field(SimField::PUSH_BOULDER)){
        return "no boulder pushed from (7, 19) north.";
    }
    sim_step_once(sim, Direction::NORTH);
    if (!(sim.state().position == MapPosition{"VictoryRoad_1F", {7, 18}})){
        return "after the push, the player is on " + to_string(sim.state().position) + ", not (7, 18).";
    }
    //  The boulder is in the way again one tile further on.
    if (!sim.use_field(SimField::PUSH_BOULDER)){
        return "the pushed boulder is not at (7, 17).";
    }
    return true;
}

//  A boulder with a wall beyond it stays, and so does the player.
UnitTestResult test_sim_boulder_stops_at_a_wall(){
    OverworldSim sim(story_maps(), MapPosition{"VictoryRoad_1F", {6, 18}}, Direction::EAST);
    if (sim.use_field(SimField::PUSH_BOULDER)){
        return "the boulder moved onto the wall at (8, 18).";
    }
    if (sim.state().violations.empty()){
        return "no rule broken by pushing a boulder into a wall.";
    }
    return true;
}

//  Nothing to push on an empty tile.
UnitTestResult test_sim_no_boulder(){
    OverworldSim sim(story_maps(), MapPosition{"VictoryRoad_1F", {11, 19}}, Direction::NORTH);
    if (sim.use_field(SimField::PUSH_BOULDER)){
        return "pushed a boulder that is not there.";
    }
    return true;
}

//  1F's switch is pressed in some number of moves, and the moves played on
//  the sim leave a boulder on the switch tile.
UnitTestResult test_1f_solution_replays_on_sim(){
    const BoulderPuzzle puzzle = victory_road_1f();
    const std::optional<std::vector<Direction>> moves = solve_boulders(map_named(puzzle.map), puzzle);
    if (!moves){
        return "no way to put a boulder on Victory Road 1F's switch (20, 16).";
    }
    OverworldSim sim(story_maps(), MapPosition{puzzle.map, puzzle.player}, Direction::NORTH);
    for (Direction direction : *moves){
        if (sim.state().facing != direction){
            sim_turn_to(sim, direction);
        }
        const TilePos ahead = sim.state().position.point.next(direction);
        const bool boulder_ahead = std::any_of(sim.objects().begin(), sim.objects().end(), [&](const MapObject& object){
            return object.name == "pushable_boulder" && object.x == ahead.x && object.y == ahead.y;
        });
        if (boulder_ahead && !sim.use_field(SimField::PUSH_BOULDER)){
            return std::format("the sim would not push at {} ({} moves).", to_string(sim.state().position), moves->size());
        }
        sim_step_once(sim, direction);
    }
    const TilePos press = puzzle.switches.front();
    const bool pressed = std::any_of(sim.objects().begin(), sim.objects().end(), [&](const MapObject& object){
        return object.name == "pushable_boulder" && object.x == press.x && object.y == press.y;
    });
    if (!pressed){
        return std::format("after {} moves, no boulder on the switch {}.", moves->size(), to_string(press));
    }
    return true;
}

//  The step as a row plays it: two tiles north from (7, 19), the boulder
//  from (7, 18) to (7, 16) and the player on (7, 17).
UnitTestResult test_push_boulder_step_on_sim(){
    OverworldSim sim(story_maps(), MapPosition{"VictoryRoad_1F", {7, 19}}, Direction::NORTH);
    StoryPose at{"VictoryRoad_1F", {7, 19}, Direction::NORTH};
    StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = "push";
    step.action = PUSH_BOULDER_VARIANT;
    step.args = {{"direction", Direction::NORTH}, {"tiles", int64_t(2)}, {"activate", false}};
    if (const std::string failure = row_step_on_sim(sim, at, step, "19.04"); !failure.empty()){
        return failure;
    }
    if (!(sim.state().position == MapPosition{"VictoryRoad_1F", {7, 17}})){
        return "after the step, on " + to_string(sim.state().position) + ", not (7, 17).";
    }
    const bool moved = std::any_of(sim.objects().begin(), sim.objects().end(), [](const MapObject& object){
        return object.name == "pushable_boulder" && object.x == 7 && object.y == 16;
    });
    if (!moved){
        return "no boulder on (7, 16).";
    }
    return true;
}

const AutoStoryBouldersCase AUTO_STORY_BOULDERS_CASES[] = {
    {
        .input = {.name = "a push_boulder step on the sim", .body = test_push_boulder_step_on_sim},
        .expectation = {.description = "two tiles pushed, the player behind the boulder"},
    },
    {
        .input = {.name = "the sim pushes a boulder one tile", .body = test_sim_pushes_a_boulder},
        .expectation = {.description = "the boulder moves north, the player takes its tile"},
    },
    {
        .input = {.name = "the sim's boulder stops at a wall", .body = test_sim_boulder_stops_at_a_wall},
        .expectation = {.description = "no push, a rule broken"},
    },
    {
        .input = {.name = "the sim has no boulder to push on a bare tile", .body = test_sim_no_boulder},
        .expectation = {.description = "no push"},
    },
    {
        .input = {.name = "Victory Road 1F's solution replays on the sim", .body = test_1f_solution_replays_on_sim},
        .expectation = {.description = "a boulder on the switch (20, 16)"},
    },
};

} // namespace


void add_tests_AutoStoryBoulders(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_BOULDERS_CASES){
        database.add<Test_AutoStoryBoulders>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
