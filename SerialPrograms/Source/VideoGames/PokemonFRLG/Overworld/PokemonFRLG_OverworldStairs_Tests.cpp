/*  Overworld Stairs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The way the game leaves a player facing after a warp onto stairs at the
 *  side of a room, an exit mat or a door (arrival_facing()), and a walk
 *  down Celadon Mansion's stairs on the sim that failed on the console.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldTileRules.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct StairsInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct StairsExpectation{
};

struct StairsCase{
    StairsInput input;
    StairsExpectation expectation;
};

class Test_OverworldStairs : public UnitTest{
public:
    explicit Test_OverworldStairs(StairsCase test_case)
        : UnitTest("PokemonFRLG::OverworldStairs - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    StairsCase m_case;
};

const std::vector<MapData>& maps(){
    static const std::vector<MapData> MAPS = overworld_maps();
    return MAPS;
}

const MapData& map_named(const std::string& name){
    for (const MapData& map : maps()){
        if (map.name == name){
            return map;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No map " + name + ".");
}

std::string facing_name(std::optional<Direction> facing){
    return facing ? direction_name(*facing) : "unchanged";
}

//  Celadon Mansion's side stairs, its front door's exit mat and Celadon
//  City's door to it: the way the game turns a player arriving on each.
UnitTestResult test_arrival_facing(){
    const struct{
        MapPosition tile;
        std::optional<Direction> facing;
    } CASES[] = {
        //  1F's stairs up, taken walking east (MB_UP_RIGHT_STAIR_WARP).
        {MapPosition{"CeladonCity_Condominiums_1F", {4, 2}}, Direction::WEST},
        //  2F's stairs up, taken walking west.
        {MapPosition{"CeladonCity_Condominiums_2F", {2, 2}}, Direction::EAST},
        //  1F's exit mat, taken walking south.
        {MapPosition{"CeladonCity_Condominiums_1F", {12, 18}}, Direction::NORTH},
        {MapPosition{"CeladonCity", map_point(maps(), "CeladonCity", "warp_3")}, Direction::SOUTH},
        {MapPosition{"CeladonCity_Condominiums_1F", {2, 2}}, std::nullopt},
    };
    for (const auto& c : CASES){
        const std::optional<Direction> facing = arrival_facing(map_named(c.tile.map), c.tile.point);
        if (facing != c.facing){
            return std::format("Arriving on {}: facing {}, expected {}.", to_string(c.tile), facing_name(facing), facing_name(c.facing));
        }
    }
    return true;
}

std::string describe(const std::vector<RouteStep>& route, const std::vector<std::string>& log){
    std::string text = "\n  route:";
    for (const RouteStep& step : route){
        text += "\n    " + to_string(step) + ", facing " + direction_name(step.facing_after) + " after";
    }
    text += "\n  log:";
    for (const std::string& line : log){
        text += "\n    " + line;
    }
    return text;
}

//  10.01 below_store_lift (console B, 2026-11-11 23:40): down Celadon
//  Mansion's stairs to 1F (4, 2), "west 2 (open)", then "north 1 (warp)"
//  onto the back door (2, 1). The game turns a player off the 2F stairs to
//  face west (pokefirered GetAdjustedInitialDirection()), so the turn tap
//  the walker made, for a player it took as facing east, was a step: west 2
//  ended on (1, 2) and north 1 met the wall, "no fade in and out".
UnitTestResult test_mansion_stairs_to_back_door(){
    const MapPosition start{"CeladonCity_Condominiums_RoofRoom", {7, 4}};
    const MapPosition goal{"CeladonCity_DepartmentStore_1F", {6, 2}};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps(), start, RouteGoal{goal, std::nullopt}, default_route_costs(), Direction::NORTH
    );
    if (!route){
        return std::string("No route from the roof room to the Department Store.");
    }
    OverworldSim sim(maps(), start, Direction::NORTH, SimScript{});
    OverworldSimWalker walker(sim, SimWalkerRules{});
    const RoutePlanning planning{.maps = &maps(), .costs = default_route_costs(), .start = start, .label = "Sim"};
    const std::string failure = walker.run(*route, planning);
    std::vector<std::string> log = walker.log();
    log.insert(log.end(), sim.state().log.begin(), sim.state().log.end());
    if (!failure.empty() || !(sim.state().position == goal) || !sim.state().violations.empty()){
        return std::format("Expected the Department Store reached by the Mansion's back door, ended at {}: \"{}\"", to_string(sim.state().position), failure)
            + describe(*route, log);
    }
    for (const RouteStep& step : *route){
        if (step.warps && step.end == MapPosition{"CeladonCity_Condominiums_1F", {4, 2}} && step.facing_after != Direction::WEST){
            return "Expected the leg down the stairs to 1F to end facing west:" + describe(*route, {});
        }
    }
    return true;
}

//  A leg on foot of `tiles` taps `direction`, to `end`.
RouteStep tapped_leg(Direction direction, int tiles, const MapPosition& end){
    RouteStep step;
    step.direction = direction;
    step.tiles = tiles;
    step.tapped = true;
    step.facing_after = direction;
    step.end = end;
    return step;
}

//  10.04 tower_3f_to_stairs (console B, 2026-11-12 03:08): taps east 11
//  along Pokemon Tower 3F's row 7 and south 3 down column 18 onto the
//  stairs (18, 10). The first south tap only turned the player, but the
//  screen check read it as a step: the leg ended on (18, 9), and the press
//  east into the stairs' warp met the wall. A tap from facing another way
//  is a turn, whatever the screen shows.
UnitTestResult test_tower_turn_read_as_step(){
    const MapPosition start{"PokemonTower_3F", {7, 7}};
    const MapPosition stairs{"PokemonTower_3F", {18, 10}};
    const MapPosition goal{"PokemonTower_4F", {18, 10}};
    std::vector<RouteStep> route{
        tapped_leg(Direction::EAST, 11, MapPosition{"PokemonTower_3F", {18, 7}}),
        tapped_leg(Direction::SOUTH, 3, stairs),
    };
    const std::optional<std::vector<RouteStep>> warp = plan_overworld_route(
        maps(), stairs, RouteGoal{goal, std::nullopt}, default_route_costs(), Direction::SOUTH
    );
    if (!warp){
        return std::string("No route from 3F's stairs to 4F.");
    }
    route.insert(route.end(), warp->begin(), warp->end());
    SimScript script;
    script.seen_turn_taps = {1};
    OverworldSim sim(maps(), start, Direction::EAST, script);
    OverworldSimWalker walker(sim, SimWalkerRules{});
    const RoutePlanning planning{.maps = &maps(), .costs = default_route_costs(), .start = start, .label = "Sim"};
    const std::string failure = walker.run(route, planning);
    std::vector<std::string> log = walker.log();
    log.insert(log.end(), sim.state().log.begin(), sim.state().log.end());
    if (!failure.empty() || !(sim.state().position == goal)){
        return std::format("Expected 4F's stairs reached, ended at {}: \"{}\"", to_string(sim.state().position), failure)
            + describe(route, log);
    }
    return true;
}

} // namespace


const StairsCase STAIRS_CASES[] = {
    {
        .input = {.name = "arriving on stairs, an exit mat or a door turns the player", .body = test_arrival_facing},
        .expectation = {},
    },
    {
        .input = {.name = "Celadon Mansion: down the stairs and out of the back door", .body = test_mansion_stairs_to_back_door},
        .expectation = {},
    },
    {
        .input = {.name = "Pokemon Tower 3F: a turn tap read as a step is not counted", .body = test_tower_turn_read_as_step},
        .expectation = {},
    },
};

void add_tests_OverworldStairs(UnitTestDatabase& database){
    for (const StairsCase& item : STAIRS_CASES){
        database.add<Test_OverworldStairs>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
