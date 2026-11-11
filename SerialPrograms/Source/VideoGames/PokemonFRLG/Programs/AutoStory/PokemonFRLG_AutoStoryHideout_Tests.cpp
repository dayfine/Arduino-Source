/*  AutoStory Rocket Hideout Steps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_CodedRooms.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_HideoutLiftRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryHideoutLift.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryFieldSteps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSpin.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct LiftInput{
    std::string name;
    std::string variant;
};
struct LiftExpectation{
    //  Nothing: the variant isn't a ride.
    std::optional<MenuPresses> presses;
};
struct LiftCase{
    LiftInput input;
    LiftExpectation expectation;
};

class Test_AutoStoryHideout : public UnitTest{
public:
    explicit Test_AutoStoryHideout(LiftCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryHideout - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string_view floors[] = {"from", "to"};
        const StoryStep step = game_step_of(m_case.input.variant, floors);
        const std::optional<LiftRide> ride = parse_lift_floor_step(step);
        const std::optional<MenuPresses>& expected = m_case.expectation.presses;
        if (!ride || !expected){
            if (ride.has_value() != expected.has_value()){
                return std::format("\"{}\": a ride {}, expected {}.", m_case.input.variant, ride.has_value(), expected.has_value());
            }
            return true;
        }
        const MenuPresses got = lift_menu_presses(*ride, rocket_hideout_lift_menu().size());
        if (got.direction != expected->direction || got.presses != expected->presses){
            return std::format(
                "\"{}\": {} press(es) {}, expected {} {}.", m_case.input.variant,
                got.presses, Pokemon::direction_name(got.direction), expected->presses, Pokemon::direction_name(expected->direction)
            );
        }
        if (!is_hideout_step(step)){
            return "A ride that isn't a hideout step.";
        }
        return true;
    }

private:
    LiftCase m_case;
};

UnitTestResult test_variants(){
    const auto game_step = [](std::string action, Pokemon::StoryActionArgs args){
        StoryStep step;
        step.kind = Pokemon::StoryStepKind::GAME;
        step.action = std::move(action);
        step.args = std::move(args);
        return step;
    };
    if (!is_hideout_step(game_step(POSTER_SWITCH_VARIANT, {})) || is_hideout_step(game_step("cut_tree", {}))
        || is_hideout_step(game_step("", {})) || is_hideout_step(game_step(POSTER_SWITCH_VARIANT, {{"from", std::string("B1F")}}))
    ){
        return "Expected only the poster switch and lift rides as hideout steps.";
    }
    const StoryStep ride = lift_floor_step("ride", "B2F", "B4F");
    const Pokemon::StoryActionArgs expected{{"from", std::string("B2F")}, {"to", std::string("B4F")}};
    if (ride.action != LIFT_FLOOR_VARIANT || ride.args != expected){
        return "Expected lift_floor with {from: B2F, to: B4F}, got \"" + ride.action + "\".";
    }
    //  A three-row menu doesn't wrap.
    const MenuPresses no_wrap = lift_menu_presses(LiftRide{0, 2}, 3);
    if (no_wrap.direction != Pokemon::Direction::SOUTH || no_wrap.presses != 2){
        return "Expected 2 down on a three-row menu.";
    }
    return true;
}

//  The table's steps, run as a row runs them on the sim: the poster's
//  switch opens the stairs north-east of it, and the lift lets out on the
//  floor picked.
std::string poster_on_sim();
std::string lift_on_sim();

UnitTestResult test_steps_on_sim(){
    const std::string poster = poster_on_sim();
    if (!poster.empty()){
        return poster;
    }
    const std::string lift = lift_on_sim();
    if (!lift.empty()){
        return lift;
    }
    return true;
}

//  The Game Corner with no poster or stairs in its map: the sim adds them
//  from code (add_coded_objects()). The room ends at x 16, so the walk
//  east ends on (15, 2), the stairs' top, only once the switch opens it.
std::string poster_on_sim(){
    std::string rows;
    for (int y = 0; y < 5; y++){
        rows += std::string(y == 0 ? "" : ", ") + (y == 0 || y == 4 ? "\"#################\"" : "\"#...............#\"");
    }
    const MapData room = parse_map(
        "{\"name\": \"CeladonCity_GameCorner\", \"source\": {\"decomp_file\": \"\", \"revision\": \"test\", \"date\": \"2026-11-09\"},"
        " \"tiles\": [" + rows + "]}",
        "CeladonCity_GameCorner"
    );
    const MapPosition below_poster{"CeladonCity_GameCorner", {GAME_CORNER_POSTER.x, GAME_CORNER_POSTER.y + 1}};
    OverworldSim sim({room}, below_poster, Direction::NORTH);
    StoryPose at{below_poster.map, below_poster.point, Direction::NORTH};
    const std::string failure = row_step_on_sim(sim, at, poster_switch_step("poster_switch"), "09.01");
    if (!failure.empty()){
        return failure;
    }
    const Milliseconds tile = MovementMechanics::TILE_TIME;
    sim.hold(Direction::EAST, tile * 6, MovementMechanics::STEP_RELEASE, tile * 6, {});
    if (!(sim.state().position == MapPosition{"CeladonCity_GameCorner", {15, 2}})){
        return "After the poster step, walked onto " + to_string(sim.state().position) + ", not the stairs' (15, 2).";
    }
    return "";
}

//  A hideout floor of 32 by 26 open tiles with its lift door, which lets
//  into the lift as pokefirered's floors' warps do (warp 1 of the lift).
MapData hideout_floor(const std::string& name, const TilePos& door){
    std::string rows;
    for (int y = 0; y < 26; y++){
        std::string row(32, '.');
        if (y == door.y){
            row[door.x] = 'D';
        }
        rows += (y == 0 ? "\"" : ", \"") + row + "\"";
    }
    return parse_map(std::format(
        "{{\"name\": \"{}\", \"source\": {{\"decomp_file\": \"\", \"revision\": \"test\", \"date\": \"2026-11-09\"}},"
        " \"tiles\": [{}], \"warps\": [{{\"x\": {}, \"y\": {}, \"enter\": \"walk_into\", \"to_map\": \"{}\", \"to_point\": \"warp_1\"}}]}}",
        name, rows, door.x, door.y, LIFT_MAP
    ), name);
}

//  The ride from B2F to B4F with the lift's room coded, not mapped: from
//  below B2F's lift door, in, the panel, out a tile south of B4F's lift
//  door (20, 23), on (20, 24): console B, 2026-11-11, 09.02's lift_to_b4f.
std::string lift_on_sim(){
    const MapPosition b4f_exit{"RocketHideout_B4F", {20, 24}};
    const std::vector<MapPosition>& exits = rocket_hideout_lift_exits();
    std::vector<MapData> maps{
        hideout_floor("RocketHideout_B2F", exits[1].point.next(Direction::NORTH)),
        hideout_floor("RocketHideout_B4F", exits[2].point.next(Direction::NORTH)),
        rocket_hideout_lift_room(),
    };
    SimScript script;
    script.lift_floors[std::string(LIFT_MAP)] = exits;
    const MapPosition below_door = exits[1];
    OverworldSim sim(maps, below_door, Direction::NORTH, script);
    StoryPose at{below_door.map, below_door.point, Direction::NORTH};
    const std::string failure = row_step_on_sim(sim, at, lift_floor_step("lift_to_b4f", "B2F", "B4F"), "09.02");
    if (!failure.empty()){
        return failure;
    }
    if (!(sim.state().position == b4f_exit)){
        return "After the lift step, the door let out on " + to_string(sim.state().position) + ".";
    }
    if (!sim.state().violations.empty()){
        return "The lift step broke the game's rules: " + sim.state().violations.front();
    }
    const Pokemon::DeclaredPose declared = Pokemon::TileDeclared{b4f_exit.map, {b4f_exit.point}, Direction::SOUTH};
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (row.id == "09.02" && step.name == "lift_to_b4f" && !(step.pose_after == declared)){
                return "Expected 09.02's lift_to_b4f to declare B4F's lift exit (20, 24).";
            }
        }
    }
    return "";
}

//  The hideout's spinners are coded: a map named as B2F or B3F slides the
//  player with no spin_* regions of its own.
UnitTestResult test_coded_spinners(){
    MapData b3f = parse_map(R"({
        "name": "RocketHideout_B3F", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": ["##########", "#........#", "#........#", "##########"]
    })", "RocketHideout_B3F");
    if (spin_direction(b3f, TilePos{4, 9}) != Direction::WEST || !stops_spinning(b3f, TilePos{2, 9})){
        return "Expected B3F (4, 9) to slide west and (2, 9) to stop a slide.";
    }
    if (coded_spin_direction("RocketHideout_B2F", TilePos{3, 4}) != Direction::WEST
        || coded_spin_direction("RocketHideout_B2F", TilePos{9, 5}) != Direction::NORTH
        || !coded_spin_stop("RocketHideout_B2F", TilePos{1, 4})
        || coded_spin_direction("RocketHideout_B1F", TilePos{3, 4}).has_value()
    ){
        return "Expected B2F's arrows and stops from pokefirered, and none on B1F.";
    }
    return true;
}

//  `legs` from `start` tapped out on the sim (RouteStep::tapped), facing
//  `facing`: as planned, and with each of the turns the taps make dropped
//  in turn (a tap facing another way only turns the player, and a
//  dropped one doesn't even turn them). Empty if each run ends on `goal`.
std::string tap_legs_on_sim(
    const MapPosition& start, Direction facing, const std::vector<Pokemon::StoryLeg>& legs, const MapPosition& goal,
    const std::vector<MapData>& maps = story_maps()
){
    std::optional<std::vector<RouteStep>> route = fixed_route(maps, start, legs);
    if (!route || !(route->back().end == goal)){
        return "The legs from " + to_string(start) + " don't end on " + to_string(goal) + ".";
    }
    for (RouteStep& step : *route){
        step.tapped = true;
    }
    for (int dropped = 0; dropped <= (int)legs.size(); dropped++){
        SimScript script;
        if (dropped > 0){
            script.dropped_turn_taps = {dropped};
        }
        OverworldSim sim(maps, start, facing, script);
        OverworldSimWalker walker(sim);
        const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = start, .label = "Sim"};
        const std::string failure = walker.run(*route, planning);
        if (!failure.empty() || !(sim.state().position == goal)){
            return std::format("With turn tap {} dropped, walked to {}, failure \"{}\".", dropped, to_string(sim.state().position), failure);
        }
    }
    return "";
}

//  The planned route from `start` to `goal` on the sim, with each of its
//  holds 1 and 2 tiles short in turn. Empty if each run ends on `goal`.
std::string short_holds_on_sim(const MapPosition& start, Direction facing, const MapPosition& goal){
    const std::vector<MapData>& maps = story_maps();
    const auto route = plan_overworld_route(maps, start, RouteGoal{goal, std::nullopt}, default_route_costs(), facing);
    if (!route){
        return "No route from " + to_string(start) + ".";
    }
    for (int hold = 1; hold <= (int)route->size(); hold++){
        for (const int short_by : {1, 2}){
            SimScript script;
            script.short_holds = {{hold, short_by}};
            OverworldSim sim(maps, start, facing, script);
            OverworldSimWalker walker(sim);
            const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = start, .label = "Sim"};
            const std::string failure = walker.run(*route, planning);
            if (!failure.empty() || !(sim.state().position == goal)){
                return std::format("Hold {} {} short from {}: walked to {}, failure \"{}\".", hold, short_by, to_string(start), to_string(sim.state().position), failure);
            }
        }
    }
    return "";
}

//  09.01's taps through B3F's arrow floor (frlg-09.01.plan.json's
//  b3f_arrow_maze and b3f_back_to_candy_row, pokeguy's way): down from
//  the stairs (18, 2) to below the stairs to B4F (15, 19), and back from
//  them (15, 18) to the stop (10, 13). The routes planned after them,
//  onto the stairs to B4F and east to below the Rare Candy, have no
//  arrow on the way: a short hold there is found again. B4F after the
//  LIFT KEY, a tap a tile from the Grunt's line to below the key and on
//  to the stairs (11, 15) with any one turn dropped, and the stairs up.
UnitTestResult test_b3f_maze_taps(){
    using Pokemon::StoryLeg;
    const MapPosition below_stairs{"RocketHideout_B3F", {15, 19}};
    const MapPosition candy_stop{"RocketHideout_B3F", {10, 13}};
    const MapPosition b4f_below_key{"RocketHideout_B4F", {3, 3}};
    const MapPosition b4f_stairs{"RocketHideout_B4F", {11, 15}};
    //  The Lift Key Grunt (4, 2) is beaten by then: (4, 3), in his line, is
    //  open floor.
    std::vector<MapData> key_grunt_beaten = story_maps();
    for (MapData& map : key_grunt_beaten){
        for (MapObject& object : map.objects){
            if (map.name == "RocketHideout_B4F" && object.x == 4 && object.y == 2){
                object.trainer.reset();
            }
        }
    }
    const std::vector<std::string> failures{
        tap_legs_on_sim({"RocketHideout_B3F", {18, 2}}, Direction::EAST, {
            {Direction::WEST, 6}, {Direction::SOUTH, 5}, {Direction::WEST, 7}, {Direction::SOUTH, 2}, {Direction::WEST, 1},
            {Direction::SOUTH, 2}, {Direction::WEST, 2}, {Direction::SOUTH, 3}, {Direction::WEST, 3}, {Direction::SOUTH, 2},
            {Direction::EAST, 2}, {Direction::WEST, 2}, {Direction::SOUTH, 5}, {Direction::EAST, 10}, {Direction::NORTH, 6},
        }, below_stairs),
        tap_legs_on_sim({"RocketHideout_B3F", {15, 18}}, Direction::WEST, {
            {Direction::SOUTH, 2}, {Direction::WEST, 5}, {Direction::NORTH, 6},
        }, candy_stop),
        short_holds_on_sim(below_stairs, Direction::NORTH, {"RocketHideout_B4F", map_point(story_maps(), "RocketHideout_B4F", "warp_0")}),
        short_holds_on_sim(candy_stop, Direction::NORTH, {"RocketHideout_B3F", {12, 13}}),
        tap_legs_on_sim({"RocketHideout_B4F", {4, 3}}, Direction::WEST, {
            {Direction::WEST, 1},
        }, b4f_below_key),
        tap_legs_on_sim(b4f_below_key, Direction::NORTH, {
            {Direction::EAST, 3}, {Direction::SOUTH, 4}, {Direction::EAST, 5}, {Direction::SOUTH, 8},
        }, b4f_stairs, key_grunt_beaten),
        short_holds_on_sim(b4f_stairs, Direction::SOUTH, {"RocketHideout_B3F", {15, 18}}),
    };
    for (const std::string& failure : failures){
        if (!failure.empty()){
            return failure;
        }
    }
    return true;
}

//  09.02's taps (frlg-09.02.plan.json): B3F from below its Rare Candy up
//  to the top corridor, and B2F's arrows to the MOON STONE's stop and on
//  to the lift room's stop (13, 20), up to row 17 and on to below the
//  lift, and B4F from the left Grunt to the right one, Giovanni and the
//  SILPH SCOPE, with any one turn dropped; the stairs up from B3F, which
//  end on their warp, with any hold short.
UnitTestResult test_b2f_maze_taps(){
    using Pokemon::StoryLeg;
    const MapPosition b2f_stairs{"RocketHideout_B2F", {21, 2}};
    const MapPosition lift_stop{"RocketHideout_B2F", {13, 20}};
    const MapPosition left_guard{"RocketHideout_B4F", {16, 15}};
    const MapPosition right_guard{"RocketHideout_B4F", {19, 15}};
    const MapPosition below_giovanni{"RocketHideout_B4F", {19, 5}};
    const std::vector<std::string> failures{
        tap_legs_on_sim({"RocketHideout_B3F", {12, 13}}, Direction::NORTH, {
            {Direction::WEST, 4}, {Direction::NORTH, 2}, {Direction::EAST, 4}, {Direction::NORTH, 7},
        }, {"RocketHideout_B3F", {12, 2}}),
        tap_legs_on_sim(b2f_stairs, Direction::EAST, {
            {Direction::SOUTH, 2}, {Direction::EAST, 2}, {Direction::SOUTH, 5}, {Direction::WEST, 6}, {Direction::NORTH, 3}, {Direction::WEST, 1},
        }, {"RocketHideout_B2F", {1, 4}}),
        tap_legs_on_sim({"RocketHideout_B2F", {2, 4}}, Direction::SOUTH, {
            {Direction::SOUTH, 4}, {Direction::EAST, 1}, {Direction::SOUTH, 1}, {Direction::EAST, 1}, {Direction::EAST, 2}, {Direction::WEST, 2},
            {Direction::EAST, 3}, {Direction::SOUTH, 2}, {Direction::WEST, 1}, {Direction::EAST, 1}, {Direction::SOUTH, 1},
        }, lift_stop),
        short_holds_on_sim({"RocketHideout_B3F", {12, 2}}, Direction::NORTH, b2f_stairs),
        tap_legs_on_sim(lift_stop, Direction::EAST, {
            {Direction::EAST, 2}, {Direction::NORTH, 3},
        }, {"RocketHideout_B2F", {15, 17}}),
        tap_legs_on_sim({"RocketHideout_B2F", {1, 4}}, Direction::WEST, {
            {Direction::EAST, 1},
        }, {"RocketHideout_B2F", {2, 4}}),
        tap_legs_on_sim({"RocketHideout_B2F", {15, 17}}, Direction::NORTH, {
            {Direction::EAST, 15}, {Direction::WEST, 2},
        }, {"RocketHideout_B2F", {28, 17}}),
        tap_legs_on_sim(left_guard, Direction::NORTH, {
            {Direction::SOUTH, 1}, {Direction::EAST, 3}, {Direction::NORTH, 1},
        }, right_guard),
        tap_legs_on_sim(right_guard, Direction::NORTH, {
            {Direction::WEST, 2}, {Direction::NORTH, 10}, {Direction::EAST, 2},
        }, below_giovanni),
        tap_legs_on_sim(below_giovanni, Direction::NORTH, {
            {Direction::WEST, 2}, {Direction::SOUTH, 2}, {Direction::EAST, 4}, {Direction::NORTH, 2},
        }, {"RocketHideout_B4F", {21, 5}}),
    };
    for (const std::string& failure : failures){
        if (!failure.empty()){
            return failure;
        }
    }
    return true;
}

//  Console B, 2026-11-11 01:05: 09.02's below_left_guard was planned from
//  the lift door (20, 23), but the door lets the player out a tile south,
//  on (20, 24). Every leg ran a row low: "west 4" ended on (16, 25), the
//  hold west on along row 25 and "east 2" left the player below the
//  potted plants (12, 24) and (13, 24), and "north 9" was blocked at once.
//  The legs now start from the lift's exit (20, 24) (lift_to_b4f declares
//  it): south to the bottom row 25, and column 16 runs up from there to the
//  Grunt (16, 14).
UnitTestResult test_b4f_left_guard_from_below_lift_door(){
    const std::vector<MapData>& maps = story_maps();
    const MapPosition lift_exit{"RocketHideout_B4F", {20, 24}};
    const MapPosition left_guard{"RocketHideout_B4F", {16, 15}};
    const StoryStep* below_left_guard = nullptr;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (row.id == "09.02" && step.name == "below_left_guard"){
                below_left_guard = &step;
            }
        }
    }
    if (below_left_guard == nullptr){
        return "Expected 09.02's step below_left_guard.";
    }
    const std::vector<Pokemon::StoryLeg> legs = Pokemon::story_arg<std::vector<Pokemon::StoryLeg>>(below_left_guard->args, "legs");
    const std::optional<std::vector<RouteStep>> route = fixed_route(maps, lift_exit, legs);
    if (!route || !(route->back().end == left_guard)){
        return "Expected the legs from the lift's exit (20, 24) to end below the left Grunt (16, 15).";
    }
    OverworldSim sim(maps, lift_exit, Direction::SOUTH, SimScript{});
    OverworldSimWalker walker(sim);
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = lift_exit, .label = "Sim"};
    const std::string failure = walker.run(*route, planning);
    if (!failure.empty() || !(sim.state().position == left_guard)){
        return std::format("Walked to {}, failure \"{}\".", to_string(sim.state().position), failure);
    }
    return true;
}

//  Console B, 2026-11-11 01:04: 09.02's below_lift tapped east past
//  (27, 17) to the wall at (30, 17), was found there, and walked the rest
//  of its legs back from (27, 17): west 3, south, east and north round a
//  square to the lift door's (28, 17). The legs are east to the wall and
//  one leg back to below the door, so an overshoot has nothing to undo.
UnitTestResult test_below_lift_one_backtrack(){
    const std::vector<MapData>& maps = story_maps();
    const MapPosition row_17{"RocketHideout_B2F", {15, 17}};
    const MapPosition wall{"RocketHideout_B2F", {30, 17}};
    const MapPosition below_door{"RocketHideout_B2F", {28, 17}};
    const StoryStep* below_lift = nullptr;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (row.id == "09.02" && step.name == "below_lift"){
                below_lift = &step;
            }
        }
    }
    if (below_lift == nullptr){
        return "Expected 09.02's step below_lift.";
    }
    const std::vector<Pokemon::StoryLeg> legs = Pokemon::story_arg<std::vector<Pokemon::StoryLeg>>(below_lift->args, "legs");
    const std::optional<std::vector<RouteStep>> route = fixed_route(maps, row_17, legs);
    if (!route || route->size() != 2
        || (*route)[0].direction != Direction::EAST || !(*route)[0].to_wall || !((*route)[0].end == wall)
        || (*route)[1].direction != Direction::WEST || !((*route)[1].end == below_door)
    ){
        return "Expected below_lift to be east to the wall (30, 17), then one leg west to (28, 17).";
    }
    const std::vector<Pokemon::StoryLeg> after_wall(legs.begin() + 1, legs.end());
    const std::vector<std::string> failures{
        tap_legs_on_sim(row_17, Direction::NORTH, legs, below_door),
        tap_legs_on_sim(wall, Direction::EAST, after_wall, below_door),
    };
    for (const std::string& failure : failures){
        if (!failure.empty()){
            return failure;
        }
    }
    return true;
}

struct AutoStoryHideoutBodyInput{
    std::string name;
    UnitTestResult (*body)();
};
struct AutoStoryHideoutBodyExpectation{
    std::string description;
};
struct AutoStoryHideoutBodyCase{
    AutoStoryHideoutBodyInput input;
    AutoStoryHideoutBodyExpectation expectation;
};

class Test_AutoStoryHideoutBody : public UnitTest{
public:
    explicit Test_AutoStoryHideoutBody(AutoStoryHideoutBodyCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryHideout - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    AutoStoryHideoutBodyCase m_case;
};

//  Console B, 2026-11-11 14:25: 09.02's b2f_arrows_to_lift_room tapped
//  "east 2" onto the arrows to the stop (14, 13), and the check after it
//  took the leg as up to 2 tiles short. Its candidates were the tiles
//  before the arrow, (8, 11) and (9, 11), not the stop the slide ends on,
//  so the hold west "to the wall (7, 11)" ran from (14, 13) onto the arrow
//  (12, 13), slid to the stop (10, 15) and stayed there (the sim walked on
//  to (9, 15)). The legs after it ran from the wrong tile, and "east 3" to
//  the wall (13, 15) was blocked after 2.
UnitTestResult test_b2f_spin_leg_check_from_its_stop(){
    const std::vector<MapData>& maps = story_maps();
    const MapPosition start{"RocketHideout_B2F", {2, 4}};
    const MapPosition lift_stop{"RocketHideout_B2F", {13, 20}};
    const std::vector<Pokemon::StoryLeg> legs{
        {Direction::SOUTH, 4}, {Direction::EAST, 1}, {Direction::SOUTH, 1}, {Direction::EAST, 1}, {Direction::EAST, 2}, {Direction::WEST, 2},
        {Direction::EAST, 3}, {Direction::SOUTH, 2}, {Direction::WEST, 1}, {Direction::EAST, 1}, {Direction::SOUTH, 1},
    };
    std::optional<std::vector<RouteStep>> route = fixed_route(maps, start, legs);
    if (!route){
        return "Expected 09.02's B2F legs to plan.";
    }
    for (RouteStep& step : *route){
        step.tapped = true;
    }
    SimScript script;
    script.holds_end_at_slide_stop = true;
    OverworldSim sim(maps, start, Direction::SOUTH, script);
    OverworldSimWalker walker(sim);
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = start, .label = "Sim"};
    const std::string failure = walker.run(*route, planning);
    if (!failure.empty() || !(sim.state().position == lift_stop)){
        return std::format("Walked to {}, failure \"{}\".", to_string(sim.state().position), failure);
    }
    return true;
}

const AutoStoryHideoutBodyCase HIDEOUT_BODY_CASES[] = {
    {
        .input = {.name = "the poster and lift variants", .body = test_variants},
        .expectation = {.description = "only the poster switch and lift rides are hideout steps"},
    },
    {
        .input = {.name = "table steps on the sim", .body = test_steps_on_sim},
        .expectation = {.description = "the poster opens the stairs; the lift lets out on the floor picked"},
    },
    {
        .input = {.name = "coded spinners", .body = test_coded_spinners},
        .expectation = {.description = "B2F's and B3F's arrows come from code, not the map"},
    },
    {
        .input = {.name = "B3F's arrow floor a tap at a time", .body = test_b3f_maze_taps},
        .expectation = {.description = "below the stairs to B4F, with any one turn dropped"},
    },
    {
        .input = {.name = "B2F's arrow floor to the lift a tap at a time", .body = test_b2f_maze_taps},
        .expectation = {.description = "09.02's taps and the routes between them, any turn dropped or hold short"},
    },
    {
        .input = {.name = "B2F's spin leg checked from its stop", .body = test_b2f_spin_leg_check_from_its_stop},
        .expectation = {.description = "09.02's B2F legs to the lift stop when a hold ends at a slide's stop"},
    },
    {
        .input = {.name = "B4F's left Grunt from the lift's exit", .body = test_b4f_left_guard_from_below_lift_door},
        .expectation = {.description = "09.02's legs from (20, 24) end below the left Grunt"},
    },
    {
        .input = {.name = "below B2F's lift: the wall, then one leg back", .body = test_below_lift_one_backtrack},
        .expectation = {.description = "east to the wall (30, 17) and west to below the door (28, 17), no loop"},
    },
};

const LiftCase LIFT_CASES[] = {
    {
        .input = {.name = "B2F to B4F: one down", .variant = "lift_floor B2F B4F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::SOUTH, 1}},
    },
    {
        //  Down past EXIT wraps to B1F: as few presses as up, and down is
        //  taken.
        .input = {.name = "B4F to B1F: two down past EXIT", .variant = "lift_floor B4F B1F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::SOUTH, 2}},
    },
    {
        .input = {.name = "B4F to B2F: one up", .variant = "lift_floor B4F B2F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::NORTH, 1}},
    },
    {
        //  B1F up past the top wraps to EXIT, then B4F: two up equal two
        //  down; down is taken.
        .input = {.name = "B1F to B4F: two down", .variant = "lift_floor B1F B4F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::SOUTH, 2}},
    },
    {
        .input = {.name = "the lift's own floor: no press", .variant = "lift_floor B4F B4F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::SOUTH, 0}},
    },
    {
        .input = {.name = "no B3F on the panel", .variant = "lift_floor B2F B3F"},
        .expectation = {.presses = std::nullopt},
    },
    {
        .input = {.name = "EXIT is no floor", .variant = "lift_floor B2F EXIT"},
        .expectation = {.presses = std::nullopt},
    },
    {
        .input = {.name = "one floor only", .variant = "lift_floor B2F"},
        .expectation = {.presses = std::nullopt},
    },
};

} // namespace


void add_tests_AutoStoryHideout(UnitTestDatabase& database){
    for (const LiftCase& item : LIFT_CASES){
        database.add<Test_AutoStoryHideout>(item);
    }
    for (const AutoStoryHideoutBodyCase& item : HIDEOUT_BODY_CASES){
        database.add<Test_AutoStoryHideoutBody>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
