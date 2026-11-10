/*  Overworld Spinner, Lift and Poster Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Rocket Hideout's walking on small made-up maps: spinner tiles that
 *  slide the player (the map functions, the planner, the sim and its
 *  walker), the lift's floor panel and the Game Corner's poster switch
 *  on the sim.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_MovementMechanics.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_OverworldSpin : public UnitTest{
public:
    Test_OverworldSpin(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::OverworldSpin - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};

//  A room with spinners:
//  - (3, 3) north, up a dead end to (3, 1) against the wall; (3, 2) is only
//    ever slid over;
//  - (5, 4) east onto (6, 4), which turns the slide south to (6, 5);
//  - (7, 3) west, onto the stop tile (5, 3).
const MapData& spinner_room(){
    static const MapData MAP = parse_map(R"({
        "name": "Spinners", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "allow_running": false,
        "tiles": ["#########", "###.#####", "###.#####", "#.......#", "#.......#", "#.......#", "#########"],
        "regions": {
            "spin_north": [[3, 3]], "spin_east": [[5, 4]], "spin_south": [[6, 4]],
            "spin_west": [[7, 3]], "spin_stop": [[5, 3]]
        }
    })", "Spinners");
    return MAP;
}

//  Two arrows that face each other.
const MapData& spinner_loop(){
    static const MapData MAP = parse_map(R"({
        "name": "SpinLoop", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": ["####", "#..#", "####"],
        "regions": {"spin_east": [[1, 1]], "spin_west": [[2, 1]]}
    })", "SpinLoop");
    return MAP;
}

std::string describe(const std::vector<RouteStep>& route){
    std::string text;
    for (const RouteStep& step : route){
        text += "\n  " + to_string(step);
    }
    return text;
}

UnitTestResult check_slide(const TilePos& from, const TilePos& end, Direction facing, int tiles){
    const std::optional<SpinSlide> slide = spin_slide(spinner_room(), PlayerState{from, ELEVATION_GROUND, false});
    if (!slide){
        return "No slide from " + to_string(from) + ".";
    }
    if (!(slide->end.position == end) || slide->facing != facing || slide->tiles != tiles){
        return std::format(
            "From {}: slid {} tile(s) to {} facing {}; expected {} to {} facing {}.",
            to_string(from), slide->tiles, to_string(slide->end.position), direction_name(slide->facing),
            tiles, to_string(end), direction_name(facing)
        );
    }
    return true;
}

UnitTestResult test_slide_into_wall(){
    return check_slide({3, 3}, {3, 1}, Direction::NORTH, 2);
}
UnitTestResult test_slide_turned_by_arrow(){
    return check_slide({5, 4}, {6, 5}, Direction::SOUTH, 2);
}
UnitTestResult test_slide_to_stop_tile(){
    return check_slide({7, 3}, {5, 3}, Direction::WEST, 2);
}
UnitTestResult test_slide_loop(){
    if (spin_slide(spinner_loop(), PlayerState{{1, 1}, ELEVATION_GROUND, false})){
        return "A slide round two arrows that face each other stopped.";
    }
    if (spin_slide(spinner_room(), PlayerState{{1, 3}, ELEVATION_GROUND, false})){
        return "A slide from a tile with no arrow.";
    }
    return true;
}

const MapPosition SPIN_START{"Spinners", {1, 5}};
const MapPosition DEAD_END{"Spinners", {3, 1}};

UnitTestResult test_route_ends_on_slide(){
    const std::vector<MapData> maps{spinner_room()};
    const auto route = plan_overworld_route(maps, SPIN_START, RouteGoal{DEAD_END, std::nullopt});
    if (!route || route->empty()){
        return std::string("No route to the dead end, which only a slide reaches.");
    }
    const RouteStep& last = route->back();
    if (last.spin_tiles != 2 || last.to_wall || !(last.end == DEAD_END)){
        return "Expected a last leg onto (3, 3) that slides 2 tiles:" + describe(*route);
    }
    const MapPosition end = follow_overworld_route(maps, SPIN_START, *route);
    if (!(end == DEAD_END)){
        return "The route replays to " + to_string(end) + ":" + describe(*route);
    }
    return true;
}

//  Walking back down from the dead end steps onto (3, 3), which slides the
//  player north again: there is no way out.
UnitTestResult test_dead_end_traps(){
    const std::vector<MapData> maps{spinner_room()};
    const auto route = plan_overworld_route(maps, DEAD_END, RouteGoal{SPIN_START, std::nullopt});
    if (route){
        return "Planned a way out past the spinner that slides back in:" + describe(*route);
    }
    return true;
}

UnitTestResult test_sim_slides(){
    OverworldSim sim({spinner_room()}, MapPosition{"Spinners", {3, 4}}, Direction::NORTH);
    sim.hold(Direction::NORTH, MovementMechanics::TILE_TIME, MovementMechanics::STEP_RELEASE, MovementMechanics::TILE_TIME, {});
    if (!(sim.state().position == DEAD_END)){
        return "One step north onto (3, 3) ended on " + to_string(sim.state().position) + ".";
    }
    if (sim.state().facing != Direction::NORTH){
        return std::string("The slide left the player facing ") + direction_name(sim.state().facing) + ".";
    }
    return true;
}

UnitTestResult test_sim_walker_walks_slide(){
    const std::vector<MapData> maps{spinner_room()};
    const auto route = plan_overworld_route(maps, SPIN_START, RouteGoal{DEAD_END, std::nullopt});
    if (!route){
        return std::string("No route.");
    }
    OverworldSim sim(maps, SPIN_START, Direction::NORTH);
    OverworldSimWalker walker(sim);
    const RoutePlanning planning{.maps = &maps, .costs = default_route_costs(), .start = SPIN_START, .label = "Sim"};
    const std::string failure = walker.run(*route, planning);
    if (!failure.empty() || !(sim.state().position == DEAD_END)){
        std::string log;
        for (const std::string& line : walker.log()){
            log += "\n  " + line;
        }
        return "Walked to " + to_string(sim.state().position) + ", failure \"" + failure + "\":" + describe(*route) + log;
    }
    return true;
}


//  A lift whose door, the exit mat (1, 2), lets out on the floor picked.
std::vector<MapData> lift_maps(){
    const char* FLOOR = R"({
        "name": "%s", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": ["###", "#.#", "###"]
    })";
    std::vector<MapData> maps;
    maps.push_back(parse_map(R"({
        "name": "Lift", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": ["###", "#.#", "#.#", "###"],
        "warps": [{"x": 1, "y": 2, "enter": "walk_south"}]
    })", "Lift"));
    for (const char* name : {"FloorA", "FloorB"}){
        std::string json = FLOOR;
        json.replace(json.find("%s"), 2, name);
        maps.push_back(parse_map(json, name));
    }
    return maps;
}

UnitTestResult lift_ride(bool pick, const MapPosition& expected){
    SimScript script;
    script.lift_floors["Lift"] = {MapPosition{"FloorA", {1, 1}}, MapPosition{"FloorB", {1, 1}}};
    OverworldSim sim(lift_maps(), MapPosition{"Lift", {1, 2}}, Direction::SOUTH, script);
    if (pick && !sim.choose_lift_floor(1)){
        return std::string("No floor 1 on the lift's panel.");
    }
    sim.hold(Direction::SOUTH, MovementMechanics::TILE_TIME, MovementMechanics::STEP_RELEASE, std::chrono::milliseconds(3000), {});
    if (!(sim.state().position == expected)){
        return "Expected " + to_string(expected) + ", ended on " + to_string(sim.state().position) + ".";
    }
    return true;
}
UnitTestResult test_lift_floor(){
    return lift_ride(true, MapPosition{"FloorB", {1, 1}});
}
UnitTestResult test_lift_no_floor(){
    return lift_ride(false, MapPosition{"Lift", {1, 2}});
}


//  The poster (2, 1) north of the player; the stairs behind it, (3, 1),
//  closed until its switch is pushed.
const MapData& poster_room(){
    static const MapData MAP = parse_map(R"({
        "name": "Corner", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": ["#####", "#...#", "#...#", "#####"],
        "objects": [
            {"name": "poster", "kind": "sign", "x": 2, "y": 1},
            {"name": "poster_stairs", "kind": "npc", "x": 3, "y": 1}
        ]
    })", "Corner");
    return MAP;
}

MapPosition poster_walk(bool push){
    OverworldSim sim({poster_room()}, MapPosition{"Corner", {2, 2}}, Direction::NORTH);
    if (push){
        sim.push_poster_switch();
    }
    const Milliseconds tile = MovementMechanics::TILE_TIME;
    //  Each hold turns the player first, which costs a tile's time.
    sim.hold(Direction::EAST, tile * 2, MovementMechanics::STEP_RELEASE, tile * 2, {});
    sim.hold(Direction::NORTH, tile * 2, MovementMechanics::STEP_RELEASE, tile * 2, {});
    return sim.state().position;
}
UnitTestResult test_poster_opens_stairs(){
    const MapPosition shut = poster_walk(false);
    if (!(shut == MapPosition{"Corner", {3, 2}})){
        return "Before the switch, walked onto " + to_string(shut) + ".";
    }
    const MapPosition open = poster_walk(true);
    if (!(open == MapPosition{"Corner", {3, 1}})){
        return "After the switch, walked onto " + to_string(open) + ", not the stairs.";
    }
    return true;
}

} // namespace


void add_tests_OverworldSpin(UnitTestDatabase& database){
    database.add<Test_OverworldSpin>("slide up to a wall", test_slide_into_wall);
    database.add<Test_OverworldSpin>("slide turned by an arrow", test_slide_turned_by_arrow);
    database.add<Test_OverworldSpin>("slide to a stop tile", test_slide_to_stop_tile);
    database.add<Test_OverworldSpin>("arrows in a loop never stop", test_slide_loop);
    database.add<Test_OverworldSpin>("route ends with a slide", test_route_ends_on_slide);
    database.add<Test_OverworldSpin>("a spinner back into a dead end traps", test_dead_end_traps);
    database.add<Test_OverworldSpin>("sim slides the player", test_sim_slides);
    database.add<Test_OverworldSpin>("sim walker walks a slide", test_sim_walker_walks_slide);
    database.add<Test_OverworldSpin>("lift lets out on the floor picked", test_lift_floor);
    database.add<Test_OverworldSpin>("lift with no floor picked stays", test_lift_no_floor);
    database.add<Test_OverworldSpin>("poster switch opens the stairs", test_poster_opens_stairs);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
