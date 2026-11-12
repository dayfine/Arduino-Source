/*  AutoStory Field Moves Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_FlyMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldTileRules.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryFieldMoveSteps.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRowSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::StoryLeg;
using Pokemon::StoryStep;

struct FieldMovesInput{
    std::string name;
    UnitTestResult (*body)();
};
struct FieldMovesExpectation{
    std::string_view description;
};
struct FieldMovesCase{
    FieldMovesInput input;
    FieldMovesExpectation expectation;
};

class Test_AutoStoryFieldMoves : public UnitTest{
public:
    explicit Test_AutoStoryFieldMoves(FieldMovesCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryFieldMoves - " + test_case.input.name)
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
    FieldMovesCase m_case;
};

bool built_in(const std::string& map){
    const std::vector<std::pair<std::string, std::string>>& maps = overworld_map_json();
    return std::any_of(maps.begin(), maps.end(), [&](const auto& entry){ return entry.first == map; });
}

//  Each town on its own cell of the fly map; on the maps built in, its
//  landing tile below a door (a Center's, or the player's house).
UnitTestResult test_fly_map(){
    std::set<std::pair<int, int>> cells;
    std::set<std::string> names;
    for (const FlyTown& town : kanto_fly_towns()){
        if (town.cell.x < 0 || town.cell.x >= FLY_MAP_COLUMNS || town.cell.y < 0 || town.cell.y >= FLY_MAP_ROWS
            || !cells.emplace(town.cell.x, town.cell.y).second || !names.insert(town.name).second
        ){
            return town.name + ": its cell is off the fly map or another town's, or its name is taken.";
        }
        if (!built_in(town.landing.map)){
            continue;
        }
        const MapData map = load_map(town.landing.map);
        const TilePos door = town.landing.point.next(Direction::NORTH);
        if (is_wall(map, town.landing.point) || !is_door(map, door)){
            return std::format("{}: no door north of the landing tile {}.", town.name, to_string(town.landing));
        }
    }
    if (kanto_fly_towns().size() != 13 || fly_town_index("CeladonCity") != 8 || fly_town_index("Celadon")){
        return "Expected Kanto's 13 Fly destinations, Celadon City 9th.";
    }
    return true;
}

//  The steps' args: a fly step needs a town Fly goes to and a party slot;
//  its menu row is SUMMARY's next unless it says. A surf step needs only
//  its way to the water.
UnitTestResult test_args(){
    StoryStep fly = fly_step("fly_celadon", 2, "CeladonCity");
    std::optional<FlyPlan> plan = parse_fly_step(fly);
    if (!plan || plan->slot != 1 || plan->menu_row != 1 || plan->town != 8){
        return "Expected slot 2, menu row 2, Celadon City.";
    }
    const Pokemon::DeclaredPose landed = Pokemon::TileDeclared{"CeladonCity", {{48, 12}}, Direction::SOUTH};
    if (!(fly.pose_after == landed)){
        return "Expected the fly step to end below Celadon's Center door, (48, 12).";
    }
    fly.args["menu_row"] = int64_t(3);
    plan = parse_fly_step(fly);
    if (!plan || plan->menu_row != 2){
        return "Expected menu row 3.";
    }
    fly.args["slot"] = int64_t(7);
    if (parse_fly_step(fly) || parse_fly_step(fly_step("fly", 1, "SeafoamIslands"))){
        return "Expected no plan for slot 7 or a town Fly doesn't go to.";
    }
    StoryStep surf = surf_step("surf", Pokemon::GridPose{"PalletTown", {{7, 17}}, Direction::SOUTH});
    if (parse_surf_step(surf) != Direction::SOUTH || !is_field_move_step(surf)){
        return "Expected a surf step south.";
    }
    surf.args["town"] = std::string("PalletTown");
    if (parse_surf_step(surf) || is_field_move_step(surf)){
        return "Expected no surf step with an arg it doesn't take.";
    }
    const std::vector<Pokemon::StoryActionSpec> specs = field_action_specs();
    const auto has = [&](const std::string& name){
        return std::any_of(specs.begin(), specs.end(), [&](const Pokemon::StoryActionSpec& spec){ return spec.name == name; });
    };
    if (!has(FLY_VARIANT) || !has(SURF_VARIANT)){
        return "Expected fly and surf among the field actions.";
    }
    return true;
}

//  Fly from Lavender Town to Celadon City on the sim, as a row runs it:
//  the player stands on Celadon's landing tile, off the Bicycle.
UnitTestResult test_fly_on_sim(){
    const MapPosition lavender{"LavenderTown", {6, 6}};
    SimScript script;
    script.on_bike = true;
    OverworldSim sim(story_maps(), lavender, Direction::SOUTH, script);
    StoryPose at{lavender.map, lavender.point, Direction::SOUTH};
    const std::string failure = row_step_on_sim(sim, at, fly_step("fly_celadon", 1, "CeladonCity"), "11.01");
    if (!failure.empty()){
        return failure;
    }
    const MapPosition celadon{"CeladonCity", {48, 12}};
    if (!(sim.state().position == celadon) || !(at.map == celadon.map && at.tile == celadon.point) || sim.state().on_bike){
        return "After Fly, on " + to_string(sim.state().position) + ", not on foot on Celadon's (48, 12).";
    }
    if (!sim.state().violations.empty()){
        return "Fly broke the game's rules: " + sim.state().violations.front();
    }
    return true;
}

StoryStep legs_step(std::string name, std::vector<StoryLeg> legs){
    StoryStep step;
    step.kind = Pokemon::StoryStepKind::LEGS;
    step.name = std::move(name);
    step.args = {{"legs", std::move(legs)}};
    return step;
}

//  Viridian City's pond, x 11 to 16 and y 26 to 30, ground west and east
//  of it. (Pallet Town's is in its Fat Man's wander box, so no leg crosses
//  it.)
const MapPosition POND_WEST_SHORE{"ViridianCity", {10, 27}};
const MapPosition POND_EAST_SHORE{"ViridianCity", {17, 27}};

//  Legs from the water: surfing, until a one-tile leg onto land. Legs
//  onto the water from land, or off it with more tiles after, aren't
//  legs.
UnitTestResult test_legs_from_water(){
    const std::vector<MapData>& maps = story_maps();
    const MapPosition pond{"ViridianCity", {11, 27}};
    const std::optional<std::vector<RouteStep>> route = fixed_route(maps, pond, {{Direction::EAST, 5}, {Direction::EAST, 1}});
    if (!route || route->size() != 2 || !(*route)[0].surfing || !(*route)[0].wild || !(*route)[1].surfing
        || !((*route)[1].end == POND_EAST_SHORE)
    ){
        return "Expected east 5 surfing, then east 1 onto Viridian City's (17, 27).";
    }
    if (fixed_route(maps, MapPosition{"ViridianCity", {16, 27}}, {{Direction::EAST, 2}})
        || fixed_route(maps, POND_WEST_SHORE, {{Direction::EAST, 1}})
    ){
        return "Expected no legs off the water with a tile after, or onto it without Surf.";
    }
    return true;
}

//  Viridian City's pond on the sim, as a row runs it: Surf east from its
//  west shore, the legs across it surfing, and off it onto the east shore.
UnitTestResult test_surf_legs_on_sim(){
    const MapPosition shore = POND_WEST_SHORE;
    OverworldSim sim(story_maps(), shore, Direction::NORTH);
    StoryPose at{shore.map, shore.point, Direction::NORTH};
    std::string failure = row_step_on_sim(sim, at, surf_step("surf", Pokemon::GridPose{"ViridianCity", {{11, 27}}, Direction::EAST}), "13.01");
    if (!failure.empty() || !sim.state().surfing){
        return "Surf: " + (failure.empty() ? std::string("not on the water.") : failure);
    }
    failure = row_step_on_sim(sim, at, legs_step("across", {{Direction::EAST, 5}, {Direction::EAST, 1}}), "13.01");
    if (!failure.empty()){
        return failure;
    }
    if (!(sim.state().position == POND_EAST_SHORE) || sim.state().surfing){
        return "After the legs, on " + to_string(sim.state().position) + ", not on land at (17, 27).";
    }
    if (!sim.state().violations.empty()){
        return "Surf broke the game's rules: " + sim.state().violations.front();
    }
    return true;
}

//  Routes planned on and onto the water, walked by the sim's walker: from
//  the water the story's routes surf to land (Surf itself is off for
//  them: RouteCost::SURF); with Surf on, a route surfs onto the pond.
UnitTestResult test_routes_on_water(){
    const std::vector<MapData>& maps = story_maps();
    OverworldSim sim(maps, POND_WEST_SHORE, Direction::EAST);
    if (!sim.use_field(SimField::SURF)){
        return "No Surf east of Viridian City's (10, 27).";
    }
    const MapPosition water = sim.state().position;
    const MapPosition land{"ViridianCity", {20, 28}};
    const std::optional<std::vector<RouteStep>> off = plan_overworld_route(maps, water, RouteGoal{land, std::nullopt}, story_route_costs());
    if (!off){
        return "No story route off the pond to (20, 28).";
    }
    std::string failure = OverworldSimWalker(sim).run(*off, RoutePlanning{.maps = &maps, .costs = story_route_costs(), .start = water, .label = "Sim"});
    if (!failure.empty() || !(sim.state().position == land) || sim.state().surfing){
        return "Off the pond, on " + to_string(sim.state().position) + ": " + failure;
    }
    const MapPosition pond{"ViridianCity", {13, 28}};
    const std::optional<std::vector<RouteStep>> onto = plan_overworld_route(maps, land, RouteGoal{pond, std::nullopt});
    const bool surfs = onto && std::any_of(onto->begin(), onto->end(), [](const RouteStep& step){ return step.action == RouteAction::SURF; });
    if (!surfs){
        return "Expected a route that surfs onto the pond.";
    }
    failure = OverworldSimWalker(sim).run(*onto, RoutePlanning{.maps = &maps, .start = land, .label = "Sim"});
    if (!failure.empty() || !(sim.state().position == pond) || !sim.state().surfing){
        return "Onto the pond, on " + to_string(sim.state().position) + ": " + failure;
    }
    return true;
}

const FieldMovesCase CASES[] = {
    {
        .input = {.name = "fly map", .body = test_fly_map},
        .expectation = {.description = "each town on its own cell, landing below a door"},
    },
    {
        .input = {.name = "step args", .body = test_args},
        .expectation = {.description = "fly and surf steps read their args, the specs list them"},
    },
    {
        .input = {.name = "fly on the sim", .body = test_fly_on_sim},
        .expectation = {.description = "Lavender Town to Celadon City's landing tile"},
    },
    {
        .input = {.name = "legs from the water", .body = test_legs_from_water},
        .expectation = {.description = "surfing legs, and a one-tile leg off the water"},
    },
    {
        .input = {.name = "surf and legs on the sim", .body = test_surf_legs_on_sim},
        .expectation = {.description = "Viridian City's pond: Surf, across, and off"},
    },
    {
        .input = {.name = "routes on the water on the sim", .body = test_routes_on_water},
        .expectation = {.description = "planned routes off and onto the pond, walked"},
    },
};

} // namespace


void add_tests_AutoStoryFieldMoves(UnitTestDatabase& database){
    for (const FieldMovesCase& test_case : CASES){
        database.add<Test_AutoStoryFieldMoves>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
