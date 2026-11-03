/*  Overworld Re-anchor Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  stop_candidates() and plan_reanchor(): finding the player again at a
 *  wall after an open leg stopped part-way.
 *
 */

#include <chrono>
#include <format>
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

std::string describe(const std::optional<Reanchor>& anchor){
    if (!anchor){
        return "none";
    }
    std::string text = to_string(anchor->found) + " after";
    for (const RouteStep& hold : anchor->holds){
        text += " " + to_string(hold);
    }
    return text + ", then" + describe(anchor->back);
}

//  How a walking leg may have stopped: anywhere (seen after the hold), and
//  at each estimate the timing can give.
std::vector<WalkedTiles> stops_of(const RouteStep& step){
    std::vector<WalkedTiles> stops{whole_leg(step)};
    for (int estimate = 0; estimate <= step.tiles; estimate++){
        stops.push_back(walked_near(step, estimate, std::chrono::milliseconds(267)));
    }
    return stops;
}

//  Plans from `start` to `goal`, then checks that the player can be found
//  again however an open leg through tall grass is stopped, and that the
//  way back reaches that leg's end.
UnitTestResult check_reanchors(const std::vector<MapData>& maps, const MapPosition& start, const RouteGoal& goal){
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, start, goal);
    if (!route){
        return "No route from " + to_string(start) + ".";
    }
    std::optional<MapPosition> at = start;
    for (const RouteStep& step : *route){
        if (step.action != RouteAction::WALK || !step.wild || step.to_wall){
            at = step.end;
            continue;
        }
        for (const WalkedTiles& walked : stops_of(step)){
            const std::vector<StopCandidate> candidates = stop_candidates(maps, at, step, walked);
            const std::optional<Reanchor> anchor = plan_reanchor(maps, candidates, step);
            if (!anchor){
                return std::format(
                    "Can't find the player again on {}, stopped {} to {} tiles in ({} candidates):{}",
                    to_string(step), walked.min, walked.max, candidates.size(), describe(*route)
                );
            }
            if (!(follow_overworld_route(maps, anchor->found, anchor->back) == step.end)){
                return "The way back from " + to_string(anchor->found) + " misses the end of " + to_string(step) + ".";
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
//  All grass, walled in by trees: where the walker was lost twice (console B,
//  2026-11-01, runs 16 and 17), and where the long dungeon walks start.
UnitTestResult test_viridian_forest(){
    const std::vector<MapData> maps = overworld_maps();
    return check_reanchors(
        maps, {"ViridianForest", {29, 61}},
        {{"Route2_ViridianForest_NorthEntrance", {7, 8}}, std::nullopt}
    );
}

//  A field with a wall at its east end and a row of ledges to the south.
const char* const TEST_FIELD_JSON = R"({
    "name": "Field", "source": {"decomp_file": "", "revision": "test", "date": "2026-10-31"},
    "tiles": ["########", "#......#", "#......#", "#vvvvvv#", "#......#"],
    "elevation": ["33333333", "33333333", "33333333", "33333333", "33333333"]
})";

//  Stopped part-way east, the player holds back west to the wall, the
//  shorter way, and walks back. Stopped part-way south above the ledges,
//  which would be hopped, they hold back north.
UnitTestResult test_rules(){
    const std::vector<MapData> maps{parse_map(TEST_FIELD_JSON, "Field")};
    RouteStep east;
    east.direction = Direction::EAST;
    east.tiles = 3;
    east.end = {"Field", {4, 1}};
    std::optional<Reanchor> anchor = plan_reanchor(maps, stop_candidates(maps, MapPosition{"Field", {1, 1}}, east, whole_leg(east)), east);
    if (!anchor || anchor->holds.size() != 1 || anchor->holds[0].direction != Direction::WEST || !(anchor->found.point == TilePos{1, 1})){
        return "Expected a hold west to (1, 1), got " + describe(anchor);
    }
    if (anchor->back.size() != 1 || !(anchor->back[0].end == east.end)){
        return "Expected one leg back east to (4, 1), got " + describe(anchor);
    }
    RouteStep south;
    south.direction = Direction::SOUTH;
    south.tiles = 1;
    south.end = {"Field", {3, 2}};
    anchor = plan_reanchor(maps, stop_candidates(maps, std::nullopt, south, whole_leg(south)), south);
    if (!anchor || anchor->holds.size() != 1 || anchor->holds[0].direction != Direction::NORTH || !(anchor->found.point == TilePos{3, 1})){
        return "Expected a hold north back to (3, 1), got " + describe(anchor);
    }
    return true;
}

//  The timing narrows a stop to a tile either side of its estimate walking,
//  two running, within the leg.
UnitTestResult test_timing(){
    RouteStep leg;
    leg.direction = Direction::EAST;
    leg.tiles = 10;
    struct Case{
        int estimate;
        int tile_ms;
        int min;
        int max;
    };
    const Case cases[] = {
        {.estimate = 5, .tile_ms = 267, .min = 4, .max = 6},
        {.estimate = 0, .tile_ms = 267, .min = 0, .max = 1},
        {.estimate = 10, .tile_ms = 267, .min = 9, .max = 10},
        {.estimate = 5, .tile_ms = 133, .min = 3, .max = 7},
        {.estimate = 1, .tile_ms = 133, .min = 0, .max = 3},
    };
    for (const Case& c : cases){
        const WalkedTiles walked = walked_near(leg, c.estimate, std::chrono::milliseconds(c.tile_ms));
        if (walked.min != c.min || walked.max != c.max){
            return std::format(
                "{} tiles in at {}ms a tile: expected {} to {}, got {} to {}.",
                c.estimate, c.tile_ms, c.min, c.max, walked.min, walked.max
            );
        }
    }
    return true;
}

//  A field with someone standing in the middle of its north row.
const char* const TEST_PERSON_JSON = R"({
    "name": "Person", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-02"},
    "tiles": ["#######", "#.....#", "#.....#", "#######"],
    "objects": [{"name": "youngster", "kind": "npc", "x": 3, "y": 1}]
})";

//  Stopped going east along the south row: holding north would meet the
//  person from one of the tiles, so the player holds along the row. On the
//  north row, past the person, they don't hold west into them.
UnitTestResult test_person_in_the_way(){
    const std::vector<MapData> maps{parse_map(TEST_PERSON_JSON, "Person")};
    RouteStep east;
    east.direction = Direction::EAST;
    east.tiles = 4;
    east.end = {"Person", {5, 2}};
    const std::optional<Reanchor> anchor = plan_reanchor(maps, stop_candidates(maps, MapPosition{"Person", {1, 2}}, east, whole_leg(east)), east);
    if (!anchor || anchor->holds.size() != 1 || anchor->holds[0].direction == Direction::NORTH){
        return "Expected one hold along the row, clear of the person, got " + describe(anchor);
    }
    RouteStep north_row = east;
    north_row.tiles = 1;
    north_row.end = {"Person", {5, 1}};
    const std::vector<StopCandidate> beside{{.player = {"Person", {4, 1}}, .trainer = std::nullopt}, {.player = {"Person", {5, 1}}, .trainer = std::nullopt}};
    const std::optional<Reanchor> blocked = plan_reanchor(maps, beside, north_row);
    if (blocked && blocked->holds[0].direction == Direction::WEST){
        return "Expected no hold west into the person, got " + describe(blocked);
    }
    return true;
}


}


void add_tests_OverworldReanchor(UnitTestDatabase& database){
    database.add<Test_OverworldReanchor>("rules", test_rules);
    database.add<Test_OverworldReanchor>("timing", test_timing);
    database.add<Test_OverworldReanchor>("a person in the way", test_person_in_the_way);
    database.add<Test_OverworldReanchor>("Viridian Mart to Oak's Lab", test_viridian_mart_to_oaks_lab);
    database.add<Test_OverworldReanchor>("Pallet Town to Viridian Mart", test_pallet_to_viridian_mart);
    database.add<Test_OverworldReanchor>("Viridian Forest", test_viridian_forest);
}



}
}
}
