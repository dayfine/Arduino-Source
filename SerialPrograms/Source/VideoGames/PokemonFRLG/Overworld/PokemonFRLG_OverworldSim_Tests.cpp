/*  Overworld Sim Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The walker's console failures (console B, 2026-11-01 and 02;
 *  ~/FRLGLab/notes/newgame-firered-console-B.md) replayed on the sim: each
 *  test walks the route that failed, the way it failed, and checks the
 *  walker now gets the player there, or stops without claiming it did.
 *  Where a walker rule fixed the failure, the test also turns the rule off
 *  and checks the sim fails the old way, so the sim is known to reproduce
 *  it.
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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_OverworldSim : public UnitTest{
public:
    Test_OverworldSim(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::OverworldSim - " + std::move(name))
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


const std::vector<MapData>& maps(){
    static const std::vector<MapData> MAPS = overworld_maps();
    return MAPS;
}

MapPosition at(const char* map, int x, int y){
    return MapPosition{map, {x, y}};
}

//  Where a warp to point `point` of `map` puts the player: there, or a tile
//  south when the game walks them out of a door.
MapPosition arrival(const char* map, const char* point){
    const TilePos tile = map_point(maps(), map, point);
    for (const MapData& data : maps()){
        if (data.name == map && walks_out_on_arrival(data, tile)){
            return MapPosition{map, tile.next(Direction::SOUTH)};
        }
    }
    return MapPosition{map, tile};
}

RouteStep leg(Direction direction, int tiles, MapPosition end, bool to_wall){
    RouteStep step;
    step.direction = direction;
    step.tiles = tiles;
    step.to_wall = to_wall;
    step.facing_after = direction;
    step.end = std::move(end);
    return step;
}

//  A walk on the sim: the route from `start` (planned to `goal` if not
//  given), what the sim starts with, and the walker's rules.
struct SimCase{
    MapPosition start = {};
    Direction facing = Direction::NORTH;
    MapPosition goal = {};
    SimScript script = {};
    SimWalkerRules rules = {};
    std::optional<std::vector<RouteStep>> route = std::nullopt;
    WallCheck wall_check = WallCheck::TAP;
    OpenLegCheck open_leg_check = OpenLegCheck::REANCHOR;
};

struct SimRun{
    std::vector<RouteStep> route;
    std::string failure;
    SimState state;
    std::vector<std::string> log;
};

std::vector<RouteStep> planned(const SimCase& c){
    if (c.route){
        return *c.route;
    }
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps(), c.start, RouteGoal{c.goal, std::nullopt}, default_route_costs(), c.facing
    );
    if (!route){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No route from " + to_string(c.start) + " to " + to_string(c.goal) + ".");
    }
    return *route;
}

SimRun run_case(const SimCase& c){
    SimRun run;
    run.route = planned(c);
    OverworldSim sim(maps(), c.start, c.facing, c.script);
    OverworldSimWalker walker(sim, c.rules);
    const RoutePlanning planning{.maps = &maps(), .costs = default_route_costs(), .start = c.start, .label = "Sim", .wall_check = c.wall_check, .open_leg_check = c.open_leg_check};
    run.failure = walker.run(run.route, planning);
    run.state = sim.state();
    run.log = walker.log();
    run.log.insert(run.log.end(), sim.state().log.begin(), sim.state().log.end());
    return run;
}

std::string describe(const std::vector<RouteStep>& route){
    std::string text;
    for (const RouteStep& step : route){
        text += "\n    " + to_string(step);
    }
    return text;
}

std::string describe(const SimRun& run){
    std::string text = std::format(
        "\n  ended at {}, facing {}, on {}; failure: \"{}\"",
        to_string(run.state.position), direction_name(run.state.facing), sim_screen_name(run.state.screen), run.failure
    );
    for (const std::string& violation : run.state.violations){
        text += "\n  violation: " + violation;
    }
    text += "\n  route:" + describe(run.route) + "\n  log:";
    for (const std::string& line : run.log){
        text += "\n    " + line;
    }
    return text;
}

//  The walk returned with the player at `goal`, and the game let it all pass.
bool arrived(const SimRun& run, const MapPosition& goal){
    return run.failure.empty() && run.state.position == goal && run.state.violations.empty();
}

//  The walk stopped with `failure`, without claiming to be anywhere.
bool stopped_with(const SimRun& run, const std::string& failure){
    return run.failure.find(failure) != std::string::npos;
}

bool on_grass(const MapPosition& tile){
    for (const MapData& map : maps()){
        if (map.name == tile.map){
            return is_tall_grass(map, tile.point);
        }
    }
    return false;
}

//  The grass steps a walk of `route` from `start` takes before step `index`
//  (from 0), and on it.
struct GrassSteps{
    int before = 0;
    int on = 0;
};
GrassSteps grass_steps(const MapPosition& start, const std::vector<RouteStep>& route, size_t index){
    GrassSteps steps;
    MapPosition from = start;
    for (size_t c = 0; c <= index && c < route.size(); c++){
        const std::vector<MapPosition> tiles = leg_tiles(maps(), from, route[c]);
        for (size_t t = 1; t < tiles.size(); t++){
            (c < index ? steps.before : steps.on) += on_grass(tiles[t]) ? 1 : 0;
        }
        from = route[c].end;
    }
    return steps;
}

bool open_wild_walk(const RouteStep& step){
    return step.action == RouteAction::WALK && step.wild && !step.warps && !step.to_wall;
}

//  The first open WALK of `route` through grass that ends on grass.
std::optional<size_t> leg_ending_on_grass(const std::vector<RouteStep>& route){
    for (size_t c = 0; c < route.size(); c++){
        if (open_wild_walk(route[c]) && on_grass(route[c].end)){
            return c;
        }
    }
    return std::nullopt;
}

//  The longest open WALK of `route` through grass.
std::optional<size_t> longest_wild_leg(const std::vector<RouteStep>& route){
    std::optional<size_t> found;
    for (size_t c = 0; c < route.size(); c++){
        if (open_wild_walk(route[c]) && (!found || route[c].tiles > route[*found].tiles)){
            found = c;
        }
    }
    return found;
}


//  cp0007 m6f and r15 (console B, 2026-11-01): out of Viridian Forest
//  through its north gate. The leg through the grass ends on the gate's
//  warp. Two rules keep it from failing on screens that aren't battles:
//  the hold isn't watched past its release, which the warp's fade comes
//  after (the tile-at-a-time build watched each step 900 ms, took the fade
//  for a battle's flashes, and tried to flee in the gate); and a battle
//  needs an HP box (black below the gate's mat read as a battle's text
//  box, and the story fought it for 10 minutes).
UnitTestResult test_forest_north_gate(){
    SimCase c{.start = at("ViridianForest", 5, 20), .facing = Direction::NORTH, .goal = at("Route2_ViridianForest_NorthEntrance", 7, 9)};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || now.state.warps != 1){
        return "Expected the gate reached:" + describe(now);
    }
    SimCase watched_past = c;
    watched_past.rules.watch_past_hold = Milliseconds(900);
    const SimRun fade = run_case(watched_past);
    if (arrived(fade, c.goal)){
        return "Expected the fade taken for a battle when the hold is watched 900 ms past its end:" + describe(fade);
    }
    SimCase text_box = c;
    text_box.rules.battle_needs_hp_box = false;
    const SimRun black = run_case(text_box);
    if (arrived(black, c.goal)){
        return "Expected the black below the gate's mat taken for a battle without an HP box:" + describe(black);
    }
    return true;
}

//  cp0007-zy (console B, 2026-11-01): with the START menu still open, three
//  legs to walls in Viridian Forest "walked" without the player moving.
//  The walker now stops before the first.
UnitTestResult test_start_menu_open(){
    SimCase c{.start = at("ViridianForest", 42, 44), .facing = Direction::NORTH, .goal = at("ViridianForest", 39, 25)};
    c.script.start_menu_open = true;
    const SimRun now = run_case(c);
    if (!stopped_with(now, "the START menu is open") || now.state.steps != 0){
        return "Expected a stop before the first leg:" + describe(now);
    }
    SimCase unchecked = c;
    unchecked.rules.check_overworld = false;
    const SimRun old = run_case(unchecked);
    if (!old.failure.empty() || !(old.state.position == c.start)){
        return "Expected the walk to claim the route done without moving, with the check off:" + describe(old);
    }
    return true;
}

//  Route 1 (console B, 2026-11-01, run 7 and r14): a battle on a leg's last
//  grass step whose flashes are missed shows only after the leg. The walker
//  watches long enough after a leg through grass to see it and run before
//  the next leg; watching 500 ms, it walked the next leg into the battle.
UnitTestResult test_battle_at_end_of_leg(){
    SimCase c{.start = at("PalletTown", 12, 1), .facing = Direction::NORTH, .goal = at("ViridianCity", 26, 27)};
    const std::vector<RouteStep> route = planned(c);
    const std::optional<size_t> wild = leg_ending_on_grass(route);
    if (!wild){
        return "Expected an open leg ending on Route 1's grass:" + describe(route);
    }
    const GrassSteps steps = grass_steps(c.start, route, *wild);
    c.script.encounters = {{.grass_step = steps.before + steps.on, .seen_late = true}};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal)){
        return "Expected the battle run from, then the route walked:" + describe(now);
    }
    SimCase short_watch = c;
    short_watch.rules.after_wild_leg_watch = Milliseconds(500);
    const SimRun old = run_case(short_watch);
    if (old.state.violations.empty()){
        return "Expected the next leg held into the battle when watching 500 ms:" + describe(old);
    }
    return true;
}

//  r16 and r17 (console B, 2026-11-01), Route 1 to Oak's Lab: a battle seen
//  only late stops an open leg through grass part-way. The walker works out
//  from the timing which tiles the player may be on, finds them again at
//  walls, and walks on to the lab's door.
UnitTestResult test_late_battle_mid_leg(){
    SimCase c{.start = at("ViridianCity_Mart", 4, 3), .facing = Direction::WEST, .goal = at("PalletTown_ProfessorOaksLab", 6, 11)};
    const std::vector<RouteStep> route = planned(c);
    const std::optional<size_t> open = longest_wild_leg(route);
    if (!open || route[*open].tiles < 16){
        return "Expected an open leg through Route 1's grass of 16 tiles or more:" + describe(route);
    }
    const GrassSteps steps = grass_steps(c.start, route, *open);
    c.script.encounters = {{.grass_step = steps.before + 1, .seen_late = true}};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal)){
        return "Expected the player found again and the lab reached:" + describe(now);
    }
    return true;
}

//  cp0007 m6f (console B, 2026-11-01), Viridian Forest: a Bug Catcher sees
//  the player, speaks, and their battle starts. It is fought; the walker
//  tried to flee it, 5 times, when it closed the message as any other.
UnitTestResult test_trainer_not_fled(){
    SimCase c{.start = at("ViridianForest", 42, 49), .facing = Direction::NORTH, .goal = at("ViridianForest", 41, 40)};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || now.state.trainers_beaten.empty()){
        return "Expected the Bug Catcher fought and the route walked:" + describe(now);
    }
    SimCase old_message = c;
    old_message.rules.fight_trainer_after_message = false;
    const SimRun old = run_case(old_message);
    if (old.state.violations.empty()){
        return "Expected the trainer's battle run from, or walked into, without the rule:" + describe(old);
    }
    return true;
}

//  cp0007-r..t (console B, 2026-11-01), Viridian City: "south 2 (to wall)"
//  from the Pokemon Center held into a ledge and hopped it, to (26, 31).
//  The planner no longer holds into ledges; the old leg still hops one.
UnitTestResult test_no_hold_over_ledge(){
    const SimCase c{.start = at("ViridianCity", 26, 27), .facing = Direction::SOUTH, .goal = at("ViridianCity", 26, 29)};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal)){
        return "Expected the route to stop above the ledge:" + describe(now);
    }
    SimCase old_leg = c;
    old_leg.route = std::vector<RouteStep>{leg(Direction::SOUTH, 2, c.goal, true)};
    old_leg.wall_check = WallCheck::SKIP;   //  the tap after it came later
    const SimRun old = run_case(old_leg);
    if (!(old.state.position == at("ViridianCity", 26, 31))){
        return "Expected the old leg to hop the ledge to (26, 31):" + describe(old);
    }
    return true;
}

//  r16 (console B, 2026-11-01), Viridian Forest: an open leg west ended 2
//  tiles short with nothing on screen, and the next leg, south to a wall,
//  was held into the TRAINER TIPS sign on (9, 29) six times. The walker now
//  finds the player at walls after the open leg, since the hold south
//  doesn't end on its tile from where a short leg leaves them. Without that
//  check it stops at the sign's second read, as off the route; without the
//  stop either, it reads the sign 6 times.
UnitTestResult test_short_leg_then_sign(){
    SimCase c{.start = at("ViridianForest", 13, 28), .facing = Direction::WEST, .goal = at("ViridianForest", 7, 33)};
    c.route = std::vector<RouteStep>{
        leg(Direction::WEST, 6, at("ViridianForest", 7, 28), false),
        leg(Direction::SOUTH, 5, c.goal, true),
    };
    c.script.short_holds[1] = 2;
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || now.state.signs_read != 0){
        return "Expected the player found at walls after the short leg, and the route walked:" + describe(now);
    }
    SimCase trusted = c;
    trusted.open_leg_check = OpenLegCheck::TRUST;
    const SimRun blocked = run_case(trusted);
    if (!stopped_with(blocked, "interrupted right away, again") || blocked.state.signs_read > 2){
        return "Expected a stop at the sign's second read without the check:" + describe(blocked);
    }
    SimCase unbounded = trusted;
    unbounded.rules.stop_if_blocked = false;
    const SimRun old = run_case(unbounded);
    if (!stopped_with(old, "Interrupted 6 times") || old.state.signs_read != 6){
        return "Expected the sign read 6 times without the rule:" + describe(old);
    }
    return true;
}

//  r16 (console B, 2026-11-01), Viridian Forest, a tile short instead of 2:
//  the hold south from (8, 28) misses the sign and stops at the wall a
//  column east of the plan's, with nothing on screen, so the walk was
//  claimed done off the route. The walker now finds the player at walls
//  after the open leg; without that check it ends on (8, 33).
UnitTestResult test_short_leg_unseen(){
    SimCase c{.start = at("ViridianForest", 13, 28), .facing = Direction::WEST, .goal = at("ViridianForest", 7, 33)};
    c.route = std::vector<RouteStep>{
        leg(Direction::WEST, 6, at("ViridianForest", 7, 28), false),
        leg(Direction::SOUTH, 5, c.goal, true),
    };
    c.script.short_holds[1] = 1;
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal)){
        return "Expected the player found at walls after the short leg, and the route walked:" + describe(now);
    }
    SimCase trusted = c;
    trusted.open_leg_check = OpenLegCheck::TRUST;
    const SimRun old = run_case(trusted);
    if (!old.failure.empty() || !(old.state.position == at("ViridianForest", 8, 33))){
        return "Expected the walk claimed done on (8, 33) without the check:" + describe(old);
    }
    return true;
}

//  r18 (console B, 2026-11-01), Pewter Gym: after Brock, Camper Liam sees
//  the player on the walk south to the door, walks up and battles. The
//  route holds on south to the door's mat; finding the player again at
//  walls failed there, since the gym's trainers block every hold.
UnitTestResult test_trainer_on_way_out(){
    const SimCase c{.start = at("PewterCity_Gym", 6, 6), .facing = Direction::NORTH, .goal = arrival("PewterCity", "warp_2")};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || now.state.trainers_beaten != std::vector<std::string>{"camper"}){
        return "Expected Liam fought and the gym left:" + describe(now);
    }
    return true;
}

//  cp0006-a (console B, 2026-11-01), Viridian Mart: the clerk's scene ends
//  with the player on (4, 3), the story's start for the walk out. From the
//  tile beside it the walk misses the only exit mat, (4, 7), and stops on
//  no fade rather than going on as if outside.
UnitTestResult test_mart_exit(){
    const SimCase c{.start = at("ViridianCity_Mart", 4, 3), .facing = Direction::WEST, .goal = arrival("ViridianCity", "warp_4")};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal)){
        return "Expected the Mart left by its mat:" + describe(now);
    }
    SimCase wrong = c;
    wrong.route = planned(c);
    wrong.start = at("ViridianCity_Mart", 5, 3);
    const SimRun off = run_case(wrong);
    if (!stopped_with(off, "no fade") || off.state.warps != 0){
        return "Expected a stop on no fade from (5, 3):" + describe(off);
    }
    return true;
}

}


void add_tests_OverworldSim(UnitTestDatabase& database){
    database.add<Test_OverworldSim>("the forest's north gate: no fade or black read as a battle", test_forest_north_gate);
    database.add<Test_OverworldSim>("the START menu left open", test_start_menu_open);
    database.add<Test_OverworldSim>("a battle at the end of a leg", test_battle_at_end_of_leg);
    database.add<Test_OverworldSim>("a late battle stops an open leg", test_late_battle_mid_leg);
    database.add<Test_OverworldSim>("a trainer's battle is fought, not fled", test_trainer_not_fled);
    database.add<Test_OverworldSim>("no hold over a ledge", test_no_hold_over_ledge);
    database.add<Test_OverworldSim>("a short leg, then a sign", test_short_leg_then_sign);
    database.add<Test_OverworldSim>("a short leg, nothing seen", test_short_leg_unseen);
    database.add<Test_OverworldSim>("a trainer on the way out of Pewter Gym", test_trainer_on_way_out);
    database.add<Test_OverworldSim>("out of Viridian Mart from the clerk's scene", test_mart_exit);
}



}
}
}
