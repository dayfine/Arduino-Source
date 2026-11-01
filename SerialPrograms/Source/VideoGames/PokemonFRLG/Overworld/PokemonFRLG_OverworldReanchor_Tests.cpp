/*  Overworld Re-anchor Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  plan_reanchor(): finding the player again at a wall after an open leg
 *  stopped part-way.
 *
 */

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldReanchor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_OverworldReanchor : public UnitTest{
public:
    Test_OverworldReanchor(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::OverworldReanchor - " + std::move(name))
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

std::string describe(const std::vector<RouteStep>& route){
    std::string text;
    for (const RouteStep& step : route){
        text += "\n  " + to_string(step);
    }
    return text;
}

//  Plans from `start` to `goal`, then checks that the player can be found
//  again after any open leg through tall grass is stopped, and that a route
//  on to the goal can be planned from there.
UnitTestResult check_reanchors(const std::vector<MapData>& maps, const MapPosition& start, const RouteGoal& goal){
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, start, goal);
    if (!route){
        return "No route from " + to_string(start) + ".";
    }
    std::optional<MapPosition> at = start;
    for (const RouteStep& step : *route){
        if (step.action == RouteAction::WALK && step.wild && !step.to_wall){
            const std::optional<Reanchor> anchor = plan_reanchor(maps, at, step);
            if (!anchor){
                return "Can't find the player again on " + to_string(step) + ":" + describe(*route);
            }
            const std::optional<std::vector<RouteStep>> rest = plan_overworld_route(maps, anchor->end, goal);
            if (!rest || !(follow_overworld_route(maps, anchor->end, *rest) == goal.position)){
                return "No route on from " + to_string(anchor->end) + ", after " + to_string(step) + ".";
            }
        }
        at = step.end;
    }
    return true;
}

//  Console B, 2026-11-01: battles on Route 1 left the walker a tile or a
//  whole leg off its count, four times.
UnitTestResult test_viridian_mart_to_oaks_lab(){
    const std::vector<MapData> maps = overworld_maps();
    return check_reanchors(
        maps, {"ViridianCity_Mart", map_point(maps, "ViridianCity_Mart", "clerk_front")},
        {{"PalletTown_ProfessorOaksLab", {6, 4}}, std::nullopt}
    );
}
UnitTestResult test_pallet_to_viridian_mart(){
    const std::vector<MapData> maps = overworld_maps();
    return check_reanchors(
        maps, {"PalletTown", map_point(maps, "PalletTown", "oaks_lab_front")},
        {{"ViridianCity_Mart", map_point(maps, "ViridianCity_Mart", "clerk_front")}, std::nullopt}
    );
}

//  A field with a wall at its east end and a row of ledges to the south.
const char* const TEST_FIELD_JSON = R"({
    "name": "Field", "width": 8, "height": 5,
    "tiles": ["########", "#......#", "#......#", "#JJJJJJ#", "#......#"],
    "elevation": ["33333333", "33333333", "33333333", "33333333", "33333333"],
    "warps": [], "objects": [],
    "connections": [], "points": [], "flag_sets": []
})";

std::string describe(const std::optional<Reanchor>& anchor){
    if (!anchor){
        return "none";
    }
    std::string text = to_string(anchor->end) + " after";
    for (const RouteStep& hold : anchor->holds){
        text += " " + to_string(hold);
    }
    return text;
}

//  Stopped part-way east, the player holds back west to the wall, the
//  shorter way. Stopped part-way south above the ledges, which would be
//  hopped, they hold back north.
UnitTestResult test_rules(){
    const std::vector<MapData> maps{parse_map(TEST_FIELD_JSON, "Field")};
    RouteStep east;
    east.direction = Direction::EAST;
    east.tiles = 3;
    east.end = {"Field", {4, 1}};
    std::optional<Reanchor> anchor = plan_reanchor(maps, MapPosition{"Field", {1, 1}}, east);
    if (!anchor || anchor->holds.size() != 1 || anchor->holds[0].direction != Direction::WEST || !(anchor->end.point == TilePos{1, 1})){
        return "Expected a hold west to (1, 1), got " + describe(anchor);
    }
    RouteStep south;
    south.direction = Direction::SOUTH;
    south.tiles = 1;
    south.end = {"Field", {3, 2}};
    anchor = plan_reanchor(maps, std::nullopt, south);
    if (!anchor || anchor->holds.size() != 1 || anchor->holds[0].direction != Direction::NORTH || !(anchor->end.point == TilePos{3, 1})){
        return "Expected a hold north back to (3, 1), got " + describe(anchor);
    }
    //  A leg to a wall is walked again instead.
    south.to_wall = true;
    if (plan_reanchor(maps, std::nullopt, south)){
        return "Expected nothing for a leg to a wall.";
    }
    return true;
}


}


void add_tests_OverworldReanchor(UnitTestDatabase& database){
    database.add<Test_OverworldReanchor>("rules", test_rules);
    database.add<Test_OverworldReanchor>("Viridian Mart to Oak's Lab", test_viridian_mart_to_oaks_lab);
    database.add<Test_OverworldReanchor>("Pallet Town to Viridian Mart", test_pallet_to_viridian_mart);
}



}
}
}
