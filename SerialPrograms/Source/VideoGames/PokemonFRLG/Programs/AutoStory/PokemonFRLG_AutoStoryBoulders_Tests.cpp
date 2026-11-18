/*  AutoStory Boulders Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Victory Road's boulder puzzles: a search over where the boulders stand
 *  (Pokemon_BoulderSolver below), and the sim's PUSH_BOULDER, which the
 *  search's answer is replayed on.
 *
 *  The maps carry each floor's barriers as `gates` and its switch's
 *  button (PokemonSimulation 1cacf81); the puzzle below repeats 1F's for
 *  the search. Rows 19.04b to 19.04d play the search's answers.
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
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_BoulderPushReader.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldGatedRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldTiles.h"
#include "PokemonFRLG_AutoStoryBoulders.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryMovedBoulders.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
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
        return std::format("after the push, the player is on {}, not (7, 18).", to_string(sim.state().position));
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

bool boulder_on(const OverworldSim& sim, TilePos tile){
    return std::any_of(sim.objects().begin(), sim.objects().end(), [&](const MapObject& object){
        return object.name == "pushable_boulder" && object.x == tile.x && object.y == tile.y;
    });
}

//  A PUSH_BOULDER_VARIANT step of `tiles` tiles `direction`, Strength
//  already on.
StoryStep push_step(Direction direction, int64_t tiles){
    StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = "push";
    step.action = PUSH_BOULDER_VARIANT;
    step.args = {{"direction", direction}, {"tiles", tiles}, {"activate", false}};
    return step;
}

//  19.04b push_south (console B, 2026-11-16): from (7, 17) the boulder on
//  (7, 18) goes to (7, 19), and the player ends on (7, 18), where the
//  plan's next walk starts.
UnitTestResult test_one_tile_push_on_sim(){
    OverworldSim sim(story_maps(), MapPosition{"VictoryRoad_1F", {7, 17}}, Direction::SOUTH);
    StoryPose at{"VictoryRoad_1F", {7, 17}, Direction::SOUTH};
    if (const std::string failure = row_step_on_sim(sim, at, push_step(Direction::SOUTH, /*tiles=*/1), "19.04b"); !failure.empty()){
        return failure;
    }
    if (!(sim.state().position == MapPosition{"VictoryRoad_1F", {7, 18}})){
        return std::format("after the push, the player is on {}, not (7, 18).", to_string(sim.state().position));
    }
    if (!boulder_on(sim, {7, 19})){
        return "no boulder on (7, 19).";
    }
    return true;
}

//  With no boulder ahead the tap to push walks the player: the step fails
//  where the player walked to.
UnitTestResult test_push_with_no_boulder_on_sim(){
    OverworldSim sim(story_maps(), MapPosition{"VictoryRoad_1F", {11, 19}}, Direction::NORTH);
    StoryPose at{"VictoryRoad_1F", {11, 19}, Direction::NORTH};
    if (row_step_on_sim(sim, at, push_step(Direction::NORTH, /*tiles=*/1), "19.04b").empty()){
        return "a push with no boulder ahead succeeded.";
    }
    if (!(sim.state().position == MapPosition{"VictoryRoad_1F", {11, 18}})){
        return std::format("the player is on {}, not (11, 18).", to_string(sim.state().position));
    }
    return true;
}

//  A boulder against a wall doesn't move: the step fails with the player
//  where they stood.
UnitTestResult test_stuck_boulder_on_sim(){
    OverworldSim sim(story_maps(), MapPosition{"VictoryRoad_1F", {6, 18}}, Direction::EAST);
    StoryPose at{"VictoryRoad_1F", {6, 18}, Direction::EAST};
    if (row_step_on_sim(sim, at, push_step(Direction::EAST, /*tiles=*/1), "19.04b").empty()){
        return "a boulder against a wall was pushed.";
    }
    if (!(sim.state().position == MapPosition{"VictoryRoad_1F", {6, 18}})){
        return std::format("the player is on {}, not (6, 18).", to_string(sim.state().position));
    }
    return true;
}

//  Taps that read as `reads` in turn, and `interrupts` cleared in turn.
class ScriptedPresses : public BoulderPresses{
public:
    ScriptedPresses(std::vector<BoulderPressRead> reads, std::vector<bool> interrupts)
        : m_reads(std::move(reads)), m_interrupts(std::move(interrupts))
    {}
    BoulderPressRead press(Direction) override{
        return m_taps < m_reads.size() ? m_reads[m_taps++] : BoulderPressRead{};
    }
    bool clear_interrupt() override{
        return m_cleared < m_interrupts.size() && m_interrupts[m_cleared++];
    }
    size_t taps() const{
        return m_taps;
    }

private:
    std::vector<BoulderPressRead> m_reads;
    std::vector<bool> m_interrupts;
    size_t m_taps = 0;
    size_t m_cleared = 0;
};

//  19.04b (console B, 2026-11-16, attempts 1 and 2 after the two-tap fix):
//  push_south from (7, 17) leaves the boulder on (7, 19) and the player on
//  (7, 18); by_boulder_2_corner's walk to (6, 18) must go round the boulder
//  where it stands now. Planned on the boulder's map tile, its first leg
//  went south onto (7, 19), bumped the boulder, and the walk ended a row off.
UnitTestResult test_walk_after_a_push_goes_round_the_boulder(){
    OverworldSim sim(story_maps(), MapPosition{"VictoryRoad_1F", {7, 17}}, Direction::SOUTH);
    StoryPose at{"VictoryRoad_1F", {7, 17}, Direction::SOUTH};
    std::set<std::string> flags;
    if (const std::string failure = row_step_carrying_flags(sim, at, push_step(Direction::SOUTH, /*tiles=*/1), "19.04b", flags); !failure.empty()){
        return failure;
    }
    StoryStep corner;
    corner.kind = Pokemon::StoryStepKind::ROUTE;
    corner.name = "by_boulder_2_corner";
    const Pokemon::GridPose target{"VictoryRoad_1F", {{6, 18}}, std::nullopt};
    corner.args = {{"target", target}, {"fight_wild", false}};
    const std::string failure = row_step_carrying_flags(sim, at, corner, "19.04b", flags);
    if (!failure.empty() || !(sim.state().position == MapPosition{"VictoryRoad_1F", {6, 18}})){
        return std::format("the walk to (6, 18) ended on {}: {}", to_string(sim.state().position), failure);
    }
    return true;
}

//  A boulder pushed twice keeps its map tile in its one flag, and the maps
//  planned on show it where it stands; leaving the map puts it back.
UnitTestResult test_boulder_flags_follow_the_boulder(){
    std::set<std::string> flags;
    note_boulder_pushed(MapPosition{"VictoryRoad_1F", {6, 19}}, Direction::EAST, /*tiles=*/2, flags);
    note_boulder_pushed(MapPosition{"VictoryRoad_1F", {8, 19}}, Direction::EAST, /*tiles=*/3, flags);
    const std::set<std::string> expected{"boulder_at:VictoryRoad_1F@7,19=12,19"};
    if (flags != expected){
        return std::format("{} flag(s), not the one for (7, 19) to (12, 19).", flags.size());
    }
    const std::vector<BoulderMove> moved = moved_boulders(flags);
    const std::vector<MapData> maps = with_boulders_moved(story_maps(), moved);
    for (const MapData& map : maps){
        if (map.name != "VictoryRoad_1F"){
            continue;
        }
        const bool there = std::any_of(map.objects.begin(), map.objects.end(), [](const MapObject& object){
            return object.name == "pushable_boulder" && object.x == 12 && object.y == 19;
        });
        if (!there){
            return "no boulder on (12, 19) in the maps.";
        }
    }
    boulders_back_after_map_change("VictoryRoad_1F", "VictoryRoad_2F", flags);
    if (!flags.empty()){
        return "a boulder flag stayed after leaving the map.";
    }
    return true;
}

//  Victory Road 1F's floor switch script copies the boulder's place to its
//  saved one (copyobjectxytoperm): leaving the floor and coming back finds
//  it still on the switch (20, 16), and its barrier open.
UnitTestResult test_boulder_on_a_switch_stays_after_leaving(){
    std::set<std::string> flags{"boulder_at:VictoryRoad_1F@7,18=20,16"};
    boulders_back_after_map_change("VictoryRoad_1F", "Route23", flags);
    if (moved_boulders(flags).size() != 1){
        return "the boulder on the switch went back to (7, 18).";
    }
    return true;
}

//  19.04b push_onto_switch (console B, 2026-11-16 01:28): the boulder
//  lands on 1F's switch (20, 16), whose script locks the player until its
//  sound ends, and the step tap after the push was lost. The step tap is
//  tried again once the overworld is quiet, and the player ends on (20, 15).
UnitTestResult test_push_onto_a_switch_waits_out_its_script(){
    const std::vector<MapData> maps = with_boulders_moved(story_maps(), {BoulderMove{.map = "VictoryRoad_1F", .from = {7, 18}, .at = {20, 15}}});
    OverworldSim sim(maps, MapPosition{"VictoryRoad_1F", {20, 14}}, Direction::SOUTH);
    sim.change_gates({});
    StoryPose at{"VictoryRoad_1F", {20, 14}, Direction::SOUTH};
    if (const std::string failure = row_step_on_sim(sim, at, push_step(Direction::SOUTH, /*tiles=*/1), "19.04b"); !failure.empty()){
        return failure;
    }
    if (!(sim.state().position == MapPosition{"VictoryRoad_1F", {20, 15}})){
        return std::format("after the push, the player is on {}, not (20, 15).", to_string(sim.state().position));
    }
    if (sim.state().gates.switches_on.empty()){
        return "no switch on.";
    }
    return true;
}

//  19.04b below_ladder (console B, 2026-11-16 01:40, run 3): from (19, 15),
//  the boulder on the switch (20, 16) and the switch on, to (4, 7) below the
//  ladder. The plan went out of the cave mouth onto Route 23 and back in,
//  which put the boulder back on (7, 18), where a leg then ran into it. The
//  route stays on Victory Road 1F.
UnitTestResult test_below_ladder_stays_on_the_floor(){
    const std::set<std::string> flags{"boulder_at:VictoryRoad_1F@7,18=20,16"};
    Pokemon::GateState gates;
    gates.switches_on.insert("VictoryRoad_1F/floor_switch_1");
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route_gated(
        run_story_maps(flags), MapPosition{"VictoryRoad_1F", {19, 15}},
        RouteGoal{MapPosition{"VictoryRoad_1F", {4, 7}}, Direction::NORTH}, gates, Pokemon::GateRules{}
    );
    if (!route){
        return "no route from (19, 15) to (4, 7).";
    }
    for (const RouteStep& step : *route){
        if (step.end.map != "VictoryRoad_1F"){
            return std::format("the route leaves Victory Road 1F for {}.", to_string(step.end));
        }
    }
    return true;
}

//  19.04d drop_to_2f (console B, 2026-11-16 03:33): push_into_hole pushed
//  3F's boulder (33, 18) east into the hole (34, 18), where it fell to 2F.
//  The maps planned on kept it standing on the hole, so the walk from
//  (33, 18) to 2F's (34, 19) went round by 3F's west room and the ladder
//  (34, 9), into Cooltrainer Colby's line (10, 17), and lost his battle.
//  The boulder is gone from 3F: the walk is one step east, down the hole.
UnitTestResult test_boulder_down_a_hole_leaves_it_open(){
    const std::set<std::string> flags{"boulder_at:VictoryRoad_3F@33,18=34,18"};
    Pokemon::GateState gates;
    gates.switches_on.insert("VictoryRoad_3F/floor_switch_1");
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route_gated(
        run_story_maps(flags), MapPosition{"VictoryRoad_3F", {33, 18}},
        RouteGoal{MapPosition{"VictoryRoad_2F", {34, 19}}, std::nullopt}, gates, Pokemon::GateRules{}
    );
    if (!route || route->empty()){
        return "no route from 3F (33, 18) to 2F (34, 19).";
    }
    if (route->size() != 1 || route->front().direction != Direction::EAST){
        return std::format("{} leg(s), the first ending on {}, not one leg east down the hole.", route->size(), to_string(route->front().end));
    }
    return true;
}

const BoulderPressRead PUSHED{.scrolled = false, .boulder_moved = true};
const BoulderPressRead WALKED{.scrolled = true, .boulder_moved = false};

//  A tile pushed is two taps: one pushes the boulder, the player in place,
//  and one walks onto the tile it left. A tap a tile pushed the boulder of
//  19.04b push_south but left the player a tile short (console B,
//  2026-11-16).
UnitTestResult test_push_taps_twice_a_tile(){
    ScriptedPresses presses({PUSHED, WALKED, PUSHED, WALKED}, {});
    const BoulderPush push = push_boulder_tiles(Direction::SOUTH, /*tiles=*/2, presses);
    if (push.failure){
        return *push.failure;
    }
    if (push.walked != 2 || presses.taps() != 4){
        return std::format("{} tile(s) walked in {} taps, not 2 in 4.", push.walked, presses.taps());
    }
    return true;
}

//  A tap to push that walks the player stops the push, with the tile
//  walked counted.
UnitTestResult test_walk_instead_of_push_is_counted(){
    ScriptedPresses presses({PUSHED, WALKED, WALKED}, {false});
    const BoulderPush push = push_boulder_tiles(Direction::SOUTH, /*tiles=*/2, presses);
    if (!push.failure || push.walked != 2){
        return std::format("failed {}, {} tile(s) walked, not failed with 2.", push.failure.has_value(), push.walked);
    }
    return true;
}

//  A wild battle set off by the step onto the boulder's old tile eats the
//  next tap to push; once it is cleared, the tap is tried again.
UnitTestResult test_cleared_interrupt_repeats_the_tap(){
    ScriptedPresses presses({PUSHED, WALKED, BoulderPressRead{}, PUSHED, WALKED}, {true});
    const BoulderPush push = push_boulder_tiles(Direction::EAST, /*tiles=*/2, presses);
    if (push.failure){
        return *push.failure;
    }
    if (push.walked != 2 || presses.taps() != 5){
        return std::format("{} tile(s) walked in {} taps, not 2 in 5.", push.walked, presses.taps());
    }
    return true;
}

//  A screen of 240x160 cave floor, scrolled `scroll_x` pixels east, the
//  player on their tile, and a boulder at (`boulder_x`, `boulder_y`).
ImageRGB32 cave_screen(size_t scroll_x, size_t boulder_x, size_t boulder_y){
    ImageRGB32 image(240, 160);
    for (size_t y = 0; y < 160; y++){
        for (size_t x = 0; x < 240; x++){
            const size_t tile = (x + scroll_x) / 16 * 7 + (y + 8) / 16 * 13;
            const uint32_t level = 0x80 + (uint32_t)(tile % 5) * 0x18;
            uint32_t pixel = 0xff000000 | (level << 16) | (level << 8) | 0x40;
            if (x >= 112 && x < 128 && y >= 72 && y < 88){
                pixel = 0xffd02020;
            }
            if (x >= boulder_x && x < boulder_x + 16 && y >= boulder_y && y < boulder_y + 16){
                pixel = 0xff304010;
            }
            image.pixel(x, y) = pixel;
        }
    }
    return image;
}

//  A boulder pushed a tile, each way, with the map still: the two tiles
//  ahead of the player changed.
UnitTestResult test_reader_sees_the_boulder_move(){
    struct Push{
        Direction direction;
        size_t from_x;
        size_t from_y;
        size_t to_x;
        size_t to_y;
    };
    const Push pushes[] = {
        {Direction::NORTH, 112, 56, 112, 40},
        {Direction::SOUTH, 112, 88, 112, 104},
        {Direction::WEST, 96, 72, 80, 72},
        {Direction::EAST, 128, 72, 144, 72},
    };
    const ImageFloatBox game(0, 0, 1, 1);
    for (const Push& push : pushes){
        const BoulderPressRead read = read_boulder_press(
            cave_screen(/*scroll_x=*/0, push.from_x, push.from_y), cave_screen(/*scroll_x=*/0, push.to_x, push.to_y), game, push.direction
        );
        if (read.scrolled || !read.boulder_moved){
            return std::format("a push {} read as scrolled {}, boulder moved {}.", direction_name(push.direction), read.scrolled, read.boulder_moved);
        }
    }
    return true;
}

//  A step scrolls the map: read as a walk. A tap that moved nothing reads
//  as nothing.
UnitTestResult test_reader_sees_a_walk_and_a_still_screen(){
    const ImageFloatBox game(0, 0, 1, 1);
    const ImageRGB32 at_rest = cave_screen(/*scroll_x=*/0, /*boulder_x=*/0, /*boulder_y=*/0);
    const ImageRGB32 stepped = cave_screen(/*scroll_x=*/16, /*boulder_x=*/0, /*boulder_y=*/0);
    const BoulderPressRead step = read_boulder_press(at_rest, stepped, game, Direction::EAST);
    if (!step.scrolled){
        return "a step east read as no scroll.";
    }
    const ImageRGB32 boulder_east = cave_screen(/*scroll_x=*/0, /*boulder_x=*/144, /*boulder_y=*/72);
    const BoulderPressRead still = read_boulder_press(boulder_east, boulder_east, game, Direction::EAST);
    if (still.scrolled || still.boulder_moved){
        return "a screen that didn't change read as a move.";
    }
    return true;
}

const AutoStoryBouldersCase AUTO_STORY_BOULDERS_CASES[] = {
    {
        .input = {.name = "19.04b's one-tile push south from (7, 17) on the sim", .body = test_one_tile_push_on_sim},
        .expectation = {.description = "the boulder on (7, 19), the player on (7, 18)"},
    },
    {
        .input = {.name = "a push step with no boulder ahead on the sim", .body = test_push_with_no_boulder_on_sim},
        .expectation = {.description = "failed, the player a tile on"},
    },
    {
        .input = {.name = "a push step of a boulder against a wall on the sim", .body = test_stuck_boulder_on_sim},
        .expectation = {.description = "failed, the player in place"},
    },
    {
        .input = {.name = "19.04b's walk to (6, 18) after the push south", .body = test_walk_after_a_push_goes_round_the_boulder},
        .expectation = {.description = "round the boulder on (7, 19), on (6, 18)"},
    },
    {
        .input = {.name = "the run flags of a boulder pushed twice, then left", .body = test_boulder_flags_follow_the_boulder},
        .expectation = {.description = "one flag, (7, 19) to (12, 19); none after leaving"},
    },
    {
        .input = {.name = "19.04b's push onto 1F's switch", .body = test_push_onto_a_switch_waits_out_its_script},
        .expectation = {.description = "the lost step tap tried again after the switch's script: on (20, 15), the switch on"},
    },
    {
        .input = {.name = "a boulder on a floor switch, then the player leaves", .body = test_boulder_on_a_switch_stays_after_leaving},
        .expectation = {.description = "the boulder stays on the switch"},
    },
    {
        .input = {.name = "19.04b's below_ladder after the switch", .body = test_below_ladder_stays_on_the_floor},
        .expectation = {.description = "a route that never leaves Victory Road 1F"},
    },
    {
        .input = {.name = "19.04d's drop_to_2f after the boulder fell down the hole", .body = test_boulder_down_a_hole_leaves_it_open},
        .expectation = {.description = "one leg east, onto the hole (34, 18)"},
    },
    {
        .input = {.name = "a push of two tiles", .body = test_push_taps_twice_a_tile},
        .expectation = {.description = "four taps: push, walk, push, walk"},
    },
    {
        .input = {.name = "a push whose second tap to push walks", .body = test_walk_instead_of_push_is_counted},
        .expectation = {.description = "failed, two tiles walked"},
    },
    {
        .input = {.name = "a push whose tap a wild battle took", .body = test_cleared_interrupt_repeats_the_tap},
        .expectation = {.description = "the battle cleared, the tap repeated, both tiles pushed"},
    },
    {
        .input = {.name = "the reader on a boulder pushed each way", .body = test_reader_sees_the_boulder_move},
        .expectation = {.description = "the boulder moved, no scroll"},
    },
    {
        .input = {.name = "the reader on a step and on a still screen", .body = test_reader_sees_a_walk_and_a_still_screen},
        .expectation = {.description = "a scroll, then nothing"},
    },
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
