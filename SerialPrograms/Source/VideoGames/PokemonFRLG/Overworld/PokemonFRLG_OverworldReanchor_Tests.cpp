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
#include "PokemonFRLG_OverworldMapsTestData.h"
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
    const std::vector<MapData> maps = test_overworld_maps();
    return check_reanchors(
        maps, {"ViridianCity_Mart", map_point(maps, "ViridianCity_Mart", "clerk_front")},
        {{"PalletTown_ProfessorOaksLab", {6, 4}}, std::nullopt}
    );
}
UnitTestResult test_pallet_to_viridian_mart(){
    const std::vector<MapData> maps = test_overworld_maps();
    return check_reanchors(
        maps, {"PalletTown", map_point(maps, "PalletTown", "oaks_lab_front")},
        {{"ViridianCity_Mart", map_point(maps, "ViridianCity_Mart", "clerk_front")}, std::nullopt}
    );
}

//  A room with a sign in the middle of its floor.
const char* const TEST_SIGNS_JSON = R"({
    "name": "Signs", "width": 8, "height": 7,
    "tiles": ["########", "#......#", "#......#", "#....S.#", "#......#", "#......#", "########"],
    "elevation": ["33333333", "33333333", "33333333", "33333333", "33333333", "33333333", "33333333"],
    "warps": [], "objects": [],
    "connections": [], "points": [], "flag_sets": []
})";

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

//  Stopped part-way east, the player may also be a row off, so one hold
//  doesn't find them: two do, the shortest being north then back west.
UnitTestResult test_rules(){
    const std::vector<MapData> maps{parse_map(TEST_FIELD_JSON, "Field")};
    RouteStep east;
    east.direction = Direction::EAST;
    east.tiles = 3;
    east.end = {"Field", {4, 1}};
    const std::optional<Reanchor> anchor = plan_reanchor(maps, MapPosition{"Field", {1, 1}}, east);
    if (!anchor || anchor->holds.size() != 2 || !(anchor->end.point == TilePos{1, 1})){
        return "Expected two holds to (1, 1), got " + describe(anchor);
    }
    return true;
}

//  Console B, 2026-11-01, run 16: "west 6" walked 2 tiles unseen, the next
//  leg went north up column 9 instead of 5, and finding the player again by
//  holding south down column 5 walked into the TRAINER TIPS sign at (9, 29)
//  six times. Every tile the player might be on now ends at one place,
//  and none of the holds walks into a sign.
UnitTestResult test_forest_exit_columns(){
    const std::vector<MapData> maps = test_overworld_maps();
    RouteStep north;
    north.direction = Direction::NORTH;
    north.tiles = 24;
    north.warps = true;
    north.wild = true;
    north.end = {"Route2_ViridianForest_NorthEntrance", {7, 10}};
    const std::optional<Reanchor> anchor = plan_reanchor(maps, MapPosition{"ViridianForest", {5, 33}}, north);
    if (!anchor || anchor->holds.empty()){
        return "Expected holds to find the player, got " + describe(anchor);
    }
    for (int x = 3; x <= 9; x++){
        MapPosition at{"ViridianForest", {x, 28}};
        for (const RouteStep& hold : anchor->holds){
            const std::optional<WallHold> wall = hold_to_wall(maps, at, false, hold.direction);
            if (!wall){
                return "From " + to_string(MapPosition{"ViridianForest", {x, 28}}) + " a hold isn't stopped by a wall: " + describe(anchor);
            }
            at = wall->end;
        }
        if (!(at == anchor->end)){
            return "From column " + std::to_string(x) + " the holds end at " + to_string(at) + ": " + describe(anchor);
        }
    }
    return true;
}

//  The same run: held south toward the wall at (5, 33), a sign spoke at
//  once, again and again. Beside a sign on the leg's line, the player is
//  found by it, if it is the only one near enough.
UnitTestResult test_found_by_sign(){
    const std::vector<MapData> maps{parse_map(TEST_SIGNS_JSON, "Signs")};
    RouteStep south;
    south.direction = Direction::SOUTH;
    south.tiles = 4;
    south.to_wall = true;
    south.end = {"Signs", {1, 5}};
    const std::optional<Reanchor> anchor = plan_reanchor(maps, std::nullopt, south, true);
    if (!anchor || !anchor->holds.empty() || !(anchor->end.point == TilePos{5, 2})){
        return "Expected to be found beside the sign at (5, 3), got " + describe(anchor);
    }
    return true;
}

}


void add_tests_OverworldReanchor(UnitTestDatabase& database){
    database.add<Test_OverworldReanchor>("rules", test_rules);
    database.add<Test_OverworldReanchor>("Viridian Mart to Oak's Lab", test_viridian_mart_to_oaks_lab);
    database.add<Test_OverworldReanchor>("Pallet Town to Viridian Mart", test_pallet_to_viridian_mart);
    database.add<Test_OverworldReanchor>("Viridian Forest exit, any column", test_forest_exit_columns);
    database.add<Test_OverworldReanchor>("found by a sign", test_found_by_sign);
}



}
}
}
