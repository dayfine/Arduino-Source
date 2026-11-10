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

#include <algorithm>
#include <cstdlib>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldLateBattle.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct OverworldSimInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct OverworldSimExpectation{
};

struct OverworldSimCase{
    OverworldSimInput input;
    OverworldSimExpectation expectation;
};

class Test_OverworldSim : public UnitTest{
public:
    explicit Test_OverworldSim(OverworldSimCase test_case)
        : UnitTest("PokemonFRLG::OverworldSim - " + test_case.input.name)
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
    OverworldSimCase m_case;
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


//  The first WALK of `route` onto a warp through grass.
std::optional<size_t> wild_warp_leg(const std::vector<RouteStep>& route){
    for (size_t c = 0; c < route.size(); c++){
        if (route[c].action == RouteAction::WALK && route[c].wild && route[c].warps){
            return c;
        }
    }
    return std::nullopt;
}

//  A walk of `c` with a wild battle, its flashes missed, on step `tile`
//  (from 1) of the route's first leg onto a warp through grass, which must
//  be a grass step. Nothing if there is no such leg or step.
std::optional<SimRun> run_with_battle_on_warp_leg(SimCase c, size_t tile){
    const std::vector<RouteStep> route = planned(c);
    const std::optional<size_t> warp = wild_warp_leg(route);
    if (!warp){
        return std::nullopt;
    }
    const MapPosition from = *warp == 0 ? c.start : route[*warp - 1].end;
    const std::vector<MapPosition> tiles = leg_tiles(maps(), from, route[*warp]);
    if (tile >= tiles.size() || !on_grass(tiles[tile])){
        return std::nullopt;
    }
    int grass_step = grass_steps(c.start, route, *warp).before;
    for (size_t t = 1; t <= tile; t++){
        grass_step += on_grass(tiles[t]) ? 1 : 0;
    }
    c.script.encounters = {{.grass_step = grass_step, .seen_late = true}};
    return run_case(c);
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

//  Console B, 2026-11-03, Mt. Moon 1F (row 00.13): a wild battle whose
//  flashes were missed started on the leg south onto the ladder; its wipe
//  passed for the ladder's fade, and the walker stopped as off the route,
//  on the leg a tile short of the ladder. Here on the leg through the
//  forest's grass onto its north gate: a battle on the leg's first or
//  second step is run from, the player found again on the leg, and the
//  gate reached through its warp: the player is on the leg's line, and
//  holds on to it. If that hold then ends well short of the warp (hold 2:
//  the leg, then the hold on), the route still stops on no fade. Without the
//  rule it stops as off the route, as on the console.
UnitTestResult test_battle_on_warp_leg(){
    const SimCase c{.start = at("ViridianForest", 5, 20), .facing = Direction::NORTH, .goal = at("Route2_ViridianForest_NorthEntrance", 7, 9)};
    for (size_t tile : {1, 2}){
        const std::optional<SimRun> now = run_with_battle_on_warp_leg(c, tile);
        if (!now){
            return std::format("Expected a leg onto the gate's warp with grass on its step {}:", tile) + describe(planned(c));
        }
        if (!arrived(*now, c.goal) || now->state.warps != 1){
            return std::format("Expected the battle on step {} run from, then the gate reached:", tile) + describe(*now);
        }
    }
    SimCase no_warp = c;
    no_warp.script.short_holds[2] = 6;
    const std::optional<SimRun> short_back = run_with_battle_on_warp_leg(no_warp, 1);
    if (!short_back || !stopped_with(*short_back, "no fade holding on toward the warp") || short_back->state.warps != 0){
        return "Expected a stop on no fade when the walk back ends short of the warp:" + (short_back ? describe(*short_back) : "");
    }
    SimCase off = c;
    off.rules.find_player_after_warp_battle = false;
    const std::optional<SimRun> old = run_with_battle_on_warp_leg(off, 1);
    if (!old || !stopped_with(*old, "a wild battle, not the warp to Route2_ViridianForest_NorthEntrance: the player is off the route.")){
        return "Expected a stop as off the route without the rule:" + (old ? describe(*old) : "");
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

//  Mt. Moon (story row 00.13): every floor tile has wild encounters, and
//  the floors join by ladders taken on stepping onto them. From the Moon
//  Stone (3, 2) on 1F down to the foot of B2F's ladder: two ladders, each
//  a fade.
UnitTestResult test_mt_moon_ladders(){
    const SimCase c{.start = at("MtMoon_1F", 3, 3), .facing = Direction::NORTH, .goal = at("MtMoon_B2F", 26, 21)};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || now.state.warps != 2){
        return "Expected B2F reached by two ladders:" + describe(now);
    }
    return true;
}

//  The Moon Stone's ball (3, 2) on Mt. Moon 1F is a short way off the way
//  down: from (11, 3), where 00.13's walk up column 11 ends, to face it
//  and then on to B1F costs a few tiles more than going straight, and
//  both walks end where they were planned to.
UnitTestResult test_moon_stone_detour(){
    const MapPosition from = at("MtMoon_1F", 11, 3);
    const MapPosition ball_front = at("MtMoon_1F", 3, 3);
    const MapPosition b1f = arrival("MtMoon_B1F", "warp_0");
    const auto tiles = [](const std::vector<RouteStep>& route){
        int count = 0;
        for (const RouteStep& step : route){
            count += step.tiles;
        }
        return count;
    };
    const SimCase straight{.start = from, .facing = Direction::NORTH, .goal = b1f};
    const SimCase to_ball{.start = from, .facing = Direction::NORTH, .goal = ball_front};
    const SimCase on{.start = ball_front, .facing = Direction::NORTH, .goal = b1f};
    const int extra = tiles(planned(to_ball)) + tiles(planned(on)) - tiles(planned(straight));
    if (extra > 8){
        return std::format("Expected the detour to cost 8 tiles or fewer, not {}:", extra) + describe(planned(to_ball)) + describe(planned(on));
    }
    for (const SimCase& c : {to_ball, on}){
        const SimRun run = run_case(c);
        if (!arrived(run, c.goal)){
            return "Expected the detour walked:" + describe(run);
        }
    }
    return true;
}

//  Mt. Moon B2F: the Grunt at (37, 21) looks around (sight 3) over the
//  only way east. 00.13 crosses his room on legs to walls; the sim's
//  trainers see every way they may look, so he sees the player on
//  (37, 22), is fought, and the leg is held on to its wall.
UnitTestResult test_mt_moon_grunt_mid_leg(){
    SimCase c{.start = at("MtMoon_B2F", 32, 22), .facing = Direction::SOUTH, .goal = at("MtMoon_B2F", 39, 14)};
    c.route = std::vector<RouteStep>{
        leg(Direction::EAST, 8, at("MtMoon_B2F", 40, 22), true),
        leg(Direction::NORTH, 3, at("MtMoon_B2F", 40, 19), true),
        leg(Direction::WEST, 1, at("MtMoon_B2F", 39, 19), false),
        leg(Direction::NORTH, 5, c.goal, true),
    };
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || now.state.trainers_beaten != std::vector<std::string>{"rocket_m"}){
        return "Expected the Grunt fought once and the legs walked:" + describe(now);
    }
    return true;
}

//  Mt. Moon B2F: 00.13's open leg east along the bottom row, (8, 38) to
//  (11, 38), ends a tile short with nothing on screen. The hold north
//  after it would end on (10, 21), a column west of the Grunt's line the
//  table steps into from (11, 21): the walker finds the player at walls
//  and walks back first. Taken as walked, the route ends on (10, 21).
UnitTestResult test_mt_moon_short_leg(){
    SimCase c{.start = at("MtMoon_B2F", 8, 38), .facing = Direction::WEST, .goal = at("MtMoon_B2F", 11, 21)};
    c.route = std::vector<RouteStep>{
        leg(Direction::EAST, 3, at("MtMoon_B2F", 11, 38), false),
        leg(Direction::NORTH, 17, c.goal, true),
    };
    c.script.short_holds[1] = 1;
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal)){
        return "Expected the player found at walls after the short leg, and the route walked:" + describe(now);
    }
    SimCase trusted = c;
    trusted.open_leg_check = OpenLegCheck::TRUST;
    const SimRun old = run_case(trusted);
    if (!old.failure.empty() || !(old.state.position == at("MtMoon_B2F", 10, 21))){
        return "Expected the walk claimed done on (10, 21) without the check:" + describe(old);
    }
    return true;
}

//  Mt. Moon: a wild battle whose flashes are missed stops a leg to a wall
//  part-way (every cave tile can start one); it is run from and the leg
//  walked again, and the route goes on down the ladders.
UnitTestResult test_mt_moon_late_battle(){
    SimCase c{.start = at("MtMoon_1F", 3, 3), .facing = Direction::NORTH, .goal = at("MtMoon_B2F", 26, 21)};
    c.script.encounters = {{.grass_step = 9, .seen_late = true}};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal)){
        return "Expected the battle run from, then B2F reached:" + describe(now);
    }
    return true;
}

//  Console B, 2026-11-03 (row 00.13, b2f_ladder_foot): a wild battle whose
//  flashes were missed started on "south 3 (warp)" from (5, 3) to 1F's
//  ladder; its wipe passed for the ladder's fade, and the walker stopped as
//  off the route, with the player on (5, 5). A battle on the leg's first or
//  second step is now run from, the player found again on the leg, and the
//  route goes on down both ladders. Without the rule it stops as it did.
UnitTestResult test_mt_moon_battle_on_ladder_leg(){
    const SimCase c{.start = at("MtMoon_1F", 3, 3), .facing = Direction::NORTH, .goal = at("MtMoon_B2F", 26, 21)};
    for (size_t tile : {1, 2}){
        const std::optional<SimRun> now = run_with_battle_on_warp_leg(c, tile);
        if (!now){
            return std::format("Expected a leg onto 1F's ladder with a cave step {}:", tile) + describe(planned(c));
        }
        if (!arrived(*now, c.goal) || now->state.warps != 2){
            return std::format("Expected the battle on step {} run from, then B1F and B2F reached:", tile) + describe(*now);
        }
    }
    SimCase off = c;
    off.rules.find_player_after_warp_battle = false;
    const std::optional<SimRun> old = run_with_battle_on_warp_leg(off, 2);
    if (!old || !stopped_with(*old, "a wild battle, not the warp to MtMoon_B1F: the player is off the route.")
        || !(old->state.position == at("MtMoon_1F", 5, 5))
    ){
        return "Expected a stop as off the route on (5, 5) without the rule:" + (old ? describe(*old) : "");
    }
    return true;
}

//  Console B, 2026-11-03 (row 00.13, b2f_ladder_foot): on B1F a wild
//  battle started part-way down "south 15 (to wall)" from (3, 3), with its
//  flashes missed. Its HP boxes came over 5 s after the step, after the
//  leg's watch had ended on the battle's empty text box: the leg was taken
//  as walked to (3, 18), and "east 19 (warp)" was walked into the battle
//  from a tile it wasn't on. The console ran (133 ms a tile) and the sim
//  walks, so the battle starts a few steps later here, for the same
//  timing. The text box after the watch is now watched on for, the leg
//  walked again to its wall, and the route reaches B2F's ladder. Without
//  the rule the leg is taken as walked, and the run stops on no fade onto
//  the ladder down to B2F, as it did (the console's walker stopped on the
//  leg; this one, which holds on toward a warp after a wild battle, stops
//  holding on).
UnitTestResult test_mt_moon_battle_seen_after_wall_leg(){
    SimCase c{.start = at("MtMoon_B1F", 3, 3), .facing = Direction::SOUTH, .goal = at("MtMoon_B2F", 26, 21)};
    c.script.encounters = {{.grass_step = 13, .seen_late = true}};
    c.script.timing.battle_opening = Milliseconds(4500);
    c.script.timing.battle_screen = Milliseconds(5300);
    const std::vector<RouteStep> route = planned(c);
    if (route.empty() || to_string(route[0]) != "walk south 15 (to wall) -> MtMoon_B1F (3, 18)"){
        return "Expected the route to start with \"south 15 (to wall)\":" + describe(route);
    }
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || now.state.warps != 1){
        return "Expected the battle run from, the leg walked again and B2F reached:" + describe(now);
    }
    SimCase off = c;
    off.rules.watch_on_for_unseen_battle = false;
    off.rules.battle_before_step = false;
    const SimRun old = run_case(off);
    const bool taken_as_walked = std::any_of(old.log.begin(), old.log.end(), [](const std::string& line){
        return line.find("south 15 (to wall): ended at MtMoon_B1F (3, 18) (walked)") != std::string::npos;
    });
    if (!taken_as_walked || !stopped_with(old, "no fade holding on toward the warp to MtMoon_B2F.")){
        return "Expected the leg taken as walked and the run stopped on no fade, as on the console, without the rule:" + describe(old);
    }
    return true;
}

//  Console B, 2026-11-03 (row 00.13, b2f_south): on Mt. Moon B2F a wild
//  battle started on the first step of "east 7 (to wall)" from (39, 14),
//  along tiles closed on their north side, which the maps didn't mark as
//  wild then: the leg watched only for messages, and the screen was black
//  after it. The leg was taken as walked to (46, 14); "south 14 (to
//  wall)" saw the battle, ran, and was held again from (40, 14), to
//  (40, 17), and every leg after it went on about 6 tiles off. The sim
//  walks the route from (39, 14) on to below Miguel's trigger (14, 12),
//  with the battle's wipe black from 1 s after its step and its HP boxes
//  at 5.3 s, as on the console.
SimCase b2f_east_battle_case(){
    SimCase c{.start = at("MtMoon_B2F", 39, 14), .facing = Direction::NORTH, .goal = at("MtMoon_B2F", 14, 12)};
    c.route = std::vector<RouteStep>{
        leg(Direction::EAST, 7, at("MtMoon_B2F", 46, 14), true),
        leg(Direction::SOUTH, 14, at("MtMoon_B2F", 46, 28), true),
        leg(Direction::WEST, 9, at("MtMoon_B2F", 37, 28), true),
        leg(Direction::SOUTH, 10, at("MtMoon_B2F", 37, 38), true),
        leg(Direction::WEST, 29, at("MtMoon_B2F", 8, 38), true),
        leg(Direction::EAST, 3, at("MtMoon_B2F", 11, 38), false),
        leg(Direction::NORTH, 17, at("MtMoon_B2F", 11, 21), true),
        leg(Direction::EAST, 3, at("MtMoon_B2F", 14, 21), false),
        leg(Direction::NORTH, 9, c.goal, false),
    };
    //  As the planner marked them on the console: all but "east 7", whose
    //  floor closed on the north the maps didn't yet mark wild.
    for (RouteStep& step : *c.route){
        step.wild = to_string(step) != "walk east 7 (to wall) -> MtMoon_B2F (46, 14)";
    }
    c.script.wild_floor_maps = {"MtMoon_B2F"};
    c.script.encounters = {{.grass_step = 1, .seen_late = true}};
    c.script.timing.battle_wipe = Milliseconds(1000);
    c.script.timing.battle_opening = Milliseconds(4500);
    c.script.timing.battle_screen = Milliseconds(5300);
    return c;
}

bool logged(const SimRun& run, const std::string& text){
    return std::any_of(run.log.begin(), run.log.end(), [&](const std::string& line){
        return line.find(text) != std::string::npos;
    });
}

//  The black after "east 7" is now a battle on its way: it is run from,
//  the leg walked again to its wall, and the route reaches (14, 12).
//  Without the rule, and without the check before the next step, the sim
//  goes the console's way: "east 7" is taken as walked, and the route ends
//  far from (14, 12).
UnitTestResult test_mt_moon_black_after_unmarked_leg(){
    const SimCase c = b2f_east_battle_case();
    const std::vector<RouteStep> planned_route = plan_overworld_route(maps(), c.start, RouteGoal{at("MtMoon_B2F", 46, 14), std::nullopt}).value_or(std::vector<RouteStep>{});
    if (planned_route.size() != 1 || to_string(planned_route[0]) != "walk east 7 (to wall) -> MtMoon_B2F (46, 14)"){
        return "Expected the planner's \"east 7 (to wall)\" from (39, 14):" + describe(planned_route);
    }
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || !logged(now, "a battle's wipe after walk east 7 (to wall)")){
        return "Expected the black after \"east 7\" watched on, the battle run from and (14, 12) reached:" + describe(now);
    }
    SimCase off = c;
    off.rules.watch_on_after_unmarked_leg = false;
    off.rules.battle_before_step = false;
    const SimRun old = run_case(off);
    const int off_by = std::abs(old.state.position.point.x - c.goal.point.x) + std::abs(old.state.position.point.y - c.goal.point.y);
    if (!logged(old, "east 7 (to wall): ended at MtMoon_B2F (46, 14) (walked)") || (old.failure.empty() && off_by < 6)){
        return "Expected \"east 7\" taken as walked and the route ended or stopped far from (14, 12), as on the console, without the rules:" + describe(old);
    }
    return true;
}

//  The same battle, its black seen only before the next step (on the
//  console the check after the leg can come before the wipe): the step
//  isn't walked, and the player is found on "east 7", walked again to its
//  wall, not on "south 14"'s line.
UnitTestResult test_mt_moon_battle_before_next_leg(){
    SimCase c = b2f_east_battle_case();
    c.rules.watch_on_after_unmarked_leg = false;
    c.wall_check = WallCheck::SKIP;
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || !logged(now, "a battle's wipe before walk south 14 (to wall)")){
        return "Expected the battle seen before \"south 14\", \"east 7\" walked again and (14, 12) reached:" + describe(now);
    }
    SimCase off = c;
    off.rules.battle_before_step = false;
    const SimRun old = run_case(off);
    if (old.state.position == c.goal){
        return "Expected the route to end off (14, 12) without the check before a step:" + describe(old);
    }
    return true;
}

//  Mt. Moon B2F, after the HELIX FOSSIL: the player steps onto (14, 7),
//  where it lay, and the way out goes north through the gap, west to the
//  ladder (5, 10), up to B1F and out of its east end onto Route 4, walked
//  a tile south of the cave mouth.
UnitTestResult test_mt_moon_way_out(){
    const SimCase c{.start = at("MtMoon_B2F", 14, 7), .facing = Direction::NORTH, .goal = at("Route4", 32, 6)};
    const SimRun now = run_case(c);
    if (!arrived(now, c.goal) || now.state.warps != 2 || now.state.facing != Direction::SOUTH){
        return "Expected Route 4 east of the mountain reached by two warps, facing south:" + describe(now);
    }
    return true;
}

//  Story row 00.14: from Route 4 east of Mt. Moon along the upper road to
//  above the ledge row 9, out of Lass Crissy's line (75, 3); one step
//  south hops the ledge onto the lower road two tiles down; from there
//  into Cerulean City and its Pokemon Center by one door. No route plans
//  past the ledge: the upper road reaches the city only by water.
UnitTestResult test_route4_to_cerulean(){
    const SimCase upper{.start = at("Route4", 32, 6), .facing = Direction::SOUTH, .goal = at("Route4", 93, 8)};
    const SimRun walk = run_case(upper);
    if (!arrived(walk, upper.goal) || !walk.state.trainers_beaten.empty()){
        return "Expected the upper road walked to above the ledge, no trainer met:" + describe(walk);
    }
    OverworldSim sim(maps(), at("Route4", 93, 8), Direction::SOUTH);
    sim.tap(Direction::SOUTH);
    sim.watch({SimWatcher::MESSAGE}, std::chrono::milliseconds(2000));
    if (!(sim.state().position == at("Route4", 93, 10))){
        return "Expected the step south to hop the ledge onto (93, 10), not " + to_string(sim.state().position) + ".";
    }
    const SimCase lower{.start = at("Route4", 93, 10), .facing = Direction::SOUTH, .goal = at("CeruleanCity_PokemonCenter_1F", 7, 4)};
    const SimRun now = run_case(lower);
    if (!arrived(now, lower.goal) || now.state.warps != 1 || !now.state.trainers_beaten.empty()){
        return "Expected Cerulean's Pokemon Center reached by its door, no trainer met:" + describe(now);
    }
    return true;
}

//  Story rows 00.14b and 00.14c in Cerulean Gym (pokefirered map.json):
//  from the Center into the Gym, up column 9 to (9, 13), west to (4, 13),
//  then to walls round the pool's west and north sides and west to (9, 7)
//  below Misty's platform, meeting no one; and back from beside her (9, 6)
//  the same way round to (9, 13) and out to the Center.
UnitTestResult test_cerulean_gym_walks(){
    const SimCase in{.start = at("CeruleanCity_PokemonCenter_1F", 7, 4), .facing = Direction::NORTH, .goal = at("CeruleanCity_Gym", 9, 13)};
    const SimRun entered = run_case(in);
    if (!arrived(entered, in.goal) || entered.state.warps != 2 || !entered.state.trainers_beaten.empty()){
        return "Expected the Gym entered and (9, 13) reached, no trainer met:" + describe(entered);
    }
    SimCase west{.start = in.goal, .facing = Direction::NORTH, .goal = at("CeruleanCity_Gym", 4, 13)};
    west.route = std::vector<RouteStep>{leg(Direction::WEST, 5, west.goal, true)};
    const SimRun walkway = run_case(west);
    if (!arrived(walkway, west.goal) || !walkway.state.trainers_beaten.empty()){
        return "Expected the wall (4, 13) reached west from (9, 13), no trainer met:" + describe(walkway);
    }
    SimCase up{.start = west.goal, .facing = Direction::WEST, .goal = at("CeruleanCity_Gym", 9, 7)};
    up.route = std::vector<RouteStep>{
        leg(Direction::NORTH, 3, at("CeruleanCity_Gym", 4, 10), true),
        leg(Direction::EAST, 8, at("CeruleanCity_Gym", 12, 10), true),
        leg(Direction::NORTH, 3, at("CeruleanCity_Gym", 12, 7), true),
        leg(Direction::WEST, 3, up.goal, false),
    };
    const SimRun walk = run_case(up);
    if (!arrived(walk, up.goal) || !walk.state.trainers_beaten.empty()){
        return "Expected (9, 7) below Misty's platform reached round the pool, no trainer met:" + describe(walk);
    }
    SimCase out{.start = at("CeruleanCity_Gym", 9, 6), .facing = Direction::WEST, .goal = at("CeruleanCity_Gym", 9, 13)};
    out.route = std::vector<RouteStep>{
        leg(Direction::SOUTH, 1, at("CeruleanCity_Gym", 9, 7), false),
        leg(Direction::EAST, 3, at("CeruleanCity_Gym", 12, 7), true),
        leg(Direction::SOUTH, 3, at("CeruleanCity_Gym", 12, 10), true),
        leg(Direction::WEST, 8, at("CeruleanCity_Gym", 4, 10), true),
        leg(Direction::SOUTH, 3, at("CeruleanCity_Gym", 4, 13), true),
        leg(Direction::EAST, 5, out.goal, true),
    };
    const SimRun back = run_case(out);
    if (!arrived(back, out.goal) || !back.state.trainers_beaten.empty()){
        return "Expected (9, 13) reached from beside Misty round the pool, no trainer met:" + describe(back);
    }
    const SimCase nurse{.start = out.goal, .facing = Direction::EAST, .goal = at("CeruleanCity_PokemonCenter_1F", 7, 4)};
    const SimRun healed = run_case(nurse);
    if (!arrived(healed, nurse.goal) || healed.state.warps != 2 || !healed.state.trainers_beaten.empty()){
        return "Expected the Center reached from (9, 13), no trainer met:" + describe(healed);
    }
    return true;
}

//  The straight ways in Cerulean Gym that 00.14b and 00.14c keep off: the
//  step onto (9, 12) meets Swimmer Luis, and the step onto (8, 7),
//  misty_front, Picnicker Diana.
UnitTestResult test_cerulean_gym_lines(){
    const struct{
        MapPosition from;
        Direction direction;
        MapPosition onto;
        const char* trainer;
    } LINES[] = {
        {at("CeruleanCity_Gym", 9, 13), Direction::NORTH, at("CeruleanCity_Gym", 9, 12), "swimmer_m_water"},
        {at("CeruleanCity_Gym", 9, 7), Direction::WEST, at("CeruleanCity_Gym", 8, 7), "picnicker"},
    };
    for (const auto& line : LINES){
        SimCase c{.start = line.from, .facing = line.direction, .goal = line.onto};
        c.route = std::vector<RouteStep>{leg(line.direction, 1, c.goal, false)};
        const SimRun now = run_case(c);
        if (!arrived(now, c.goal) || now.state.trainers_beaten != std::vector<std::string>{line.trainer}){
            return std::format("Expected the {} fought on the step onto {}:", line.trainer, to_string(line.onto)) + describe(now);
        }
    }
    return true;
}

//  Story rows 00.15 and 00.16: out of Cerulean's Pokemon Center by its mat
//  to below the rival's trigger row, and on north across the city's edge
//  onto Nugget Bridge, below Cale's row, meeting no one.
UnitTestResult test_cerulean_to_bridge(){
    const MapPosition nurse = at("CeruleanCity_PokemonCenter_1F", 7, 4);
    for (const MapPosition& goal : {at("CeruleanCity", 23, 7), at("Route24", 11, 32)}){
        const SimCase c{.start = nurse, .facing = Direction::NORTH, .goal = goal};
        const SimRun now = run_case(c);
        if (!arrived(now, c.goal) || now.state.warps != 1 || !now.state.trainers_beaten.empty()){
            return "Expected " + to_string(goal) + " reached by the Center's mat, no trainer met:" + describe(now);
        }
    }
    return true;
}

//  Story row 00.16 up Nugget Bridge: the short legs between the five
//  trainers' rows meet no one; and the step from the tile below each row
//  onto it is in that trainer's sight, beside them, so they fight there
//  without walking: Cale, Ali, Timmy, Reli and Ethan.
UnitTestResult test_nugget_bridge_legs(){
    SimCase between{.start = at("Route24", 11, 31), .facing = Direction::NORTH, .goal = at("Route24", 11, 29)};
    between.route = std::vector<RouteStep>{leg(Direction::NORTH, 2, between.goal, false)};
    const SimRun walk = run_case(between);
    if (!arrived(walk, between.goal) || !walk.state.trainers_beaten.empty()){
        return "Expected the leg between two rows walked, no trainer met:" + describe(walk);
    }
    const struct{
        int row;
        const char* trainer;
    } LINES[] = {{31, "bug_catcher"}, {28, "lass"}, {25, "youngster"}, {22, "lass"}, {19, "camper"}};
    for (const auto& line : LINES){
        SimCase c{.start = at("Route24", 11, line.row + 1), .facing = Direction::NORTH, .goal = at("Route24", 11, line.row)};
        c.route = std::vector<RouteStep>{leg(Direction::NORTH, 1, c.goal, false)};
        const SimRun now = run_case(c);
        if (!arrived(now, c.goal) || now.state.trainers_beaten != std::vector<std::string>{line.trainer}){
            return std::format("Expected the {} on row {} fought beside the player:", line.trainer, line.row) + describe(now);
        }
    }
    return true;
}

} // namespace



//  Story row 00.24 from Route 6 to Vermilion City's nurse: the open leg
//  south 14 crosses onto Vermilion City and is held a tile longer for the
//  game holding the player up there. It didn't: the player ended a tile
//  south of (23, 7), the leg west on (15, 8), and the north 1 onto the
//  Center's door only reached the tile below it: "no fade in and out"
//  (console B, 2026-11-03). Either way the door is reached.
UnitTestResult test_route6_to_vermilion_center(){
    const MapPosition nurse = at("VermilionCity_PokemonCenter_1F", 7, 4);
    for (const bool stalls : {true, false}){
        SimCase c{.start = at("Route6", 11, 33), .facing = Direction::EAST, .goal = nurse};
        c.script.connections_stall = stalls;
        const SimRun now = run_case(c);
        if (!arrived(now, c.goal) || now.state.warps != 1){
            return std::format("Expected the nurse reached by the Center's door, the crossing {}:", stalls ? "stalled" : "not stalled") + describe(now);
        }
    }
    return true;
}

const OverworldSimCase OVERWORLD_SIM_CASES[] = {
    {.input = {"the forest's north gate: no fade or black read as a battle", test_forest_north_gate}, .expectation = {},},
    {.input = {"a battle on a leg onto a warp", test_battle_on_warp_leg}, .expectation = {},},
    {.input = {"the START menu left open", test_start_menu_open}, .expectation = {},},
    {.input = {"a battle at the end of a leg", test_battle_at_end_of_leg}, .expectation = {},},
    {.input = {"a late battle stops an open leg", test_late_battle_mid_leg}, .expectation = {},},
    {.input = {"a trainer's battle is fought, not fled", test_trainer_not_fled}, .expectation = {},},
    {.input = {"no hold over a ledge", test_no_hold_over_ledge}, .expectation = {},},
    {.input = {"a short leg, then a sign", test_short_leg_then_sign}, .expectation = {},},
    {.input = {"a short leg, nothing seen", test_short_leg_unseen}, .expectation = {},},
    {.input = {"a trainer on the way out of Pewter Gym", test_trainer_on_way_out}, .expectation = {},},
    {.input = {"out of Viridian Mart from the clerk's scene", test_mart_exit}, .expectation = {},},
    {.input = {"Mt. Moon: down the ladders to B2F", test_mt_moon_ladders}, .expectation = {},},
    {.input = {"Mt. Moon: the Moon Stone is a short detour", test_moon_stone_detour}, .expectation = {},},
    {.input = {"Mt. Moon: the Grunt who looks around stops a leg to a wall", test_mt_moon_grunt_mid_leg}, .expectation = {},},
    {.input = {"Mt. Moon: a short open leg before the Grunt's line", test_mt_moon_short_leg}, .expectation = {},},
    {.input = {"Mt. Moon: a late wild battle in the cave", test_mt_moon_late_battle}, .expectation = {},},
    {.input = {"Mt. Moon: after the HELIX, out to Route 4", test_mt_moon_way_out}, .expectation = {},},
    {.input = {"Mt. Moon: a battle on the leg onto 1F's ladder", test_mt_moon_battle_on_ladder_leg}, .expectation = {},},
    {.input = {"Mt. Moon: a battle seen only after a leg to a wall", test_mt_moon_battle_seen_after_wall_leg}, .expectation = {},},
    {.input = {"Mt. Moon: black after a leg not marked wild", test_mt_moon_black_after_unmarked_leg}, .expectation = {},},
    {.input = {"Mt. Moon: a battle seen only before the next leg", test_mt_moon_battle_before_next_leg}, .expectation = {},},
    {.input = {"Route 4 east, down its ledge, to Cerulean's Pokemon Center", test_route4_to_cerulean}, .expectation = {},},
    {.input = {"Cerulean Gym: to Misty and back round the pool, no trainer met", test_cerulean_gym_walks}, .expectation = {},},
    {.input = {"Cerulean Gym: the straight ways meet Luis and Diana", test_cerulean_gym_lines}, .expectation = {},},
    {.input = {"Cerulean: the Center to the rival's row and Nugget Bridge", test_cerulean_to_bridge}, .expectation = {},},
    {.input = {"Nugget Bridge: each trainer met on the step onto their row", test_nugget_bridge_legs}, .expectation = {},},    {.input = {"Route 6 into Vermilion's Pokemon Center, the crossing stalled or not", test_route6_to_vermilion_center}, .expectation = {}},
};

void add_tests_OverworldSim(UnitTestDatabase& database){
    for (const auto& item : OVERWORLD_SIM_CASES){
        database.add<Test_OverworldSim>(item);
    }
    add_tests_OverworldLateBattle(database);
    add_tests_OverworldWarpFade(database);
    add_tests_OverworldSpin(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation