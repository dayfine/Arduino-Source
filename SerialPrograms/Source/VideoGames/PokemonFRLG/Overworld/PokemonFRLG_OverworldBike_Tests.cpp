/*  Overworld Bicycle Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Bicycle on the sim, on small made-up maps: its taps, each a tile
 *  and some dropped when they come too fast, open legs counted out a tap
 *  at a time, and SELECT getting on (console B, 2026-11-09).
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_MovementMechanics.h"
#include "PokemonFRLG_BikeSpinPresses.h"
#include "PokemonFRLG_BikeSpinMechanics.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_OverworldRecoveryWalk.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldSimField.h"
#include "PokemonFRLG_OverworldBike.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using namespace MovementMechanics;
using OverworldWalkSteps::BikeSelectCalls;

struct BikeInput{
    std::string name;
    UnitTestResult (*body)();
};
struct BikeExpectation{
    std::string description;
};
struct BikeCase{
    BikeInput input;
    BikeExpectation expectation;
};

class Test_OverworldBike : public UnitTest{
public:
    explicit Test_OverworldBike(BikeCase test_case)
        : UnitTest("PokemonFRLG::OverworldBike - " + test_case.input.name)
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
    BikeCase m_case;
};

//  A field 28 tiles wide with a rock at (7, 2).
const std::vector<MapData>& field(){
    static const std::vector<MapData> MAPS{parse_map(R"({
        "name": "BikeField", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": [
            "##############################",
            "#............................#",
            "#......#.....................#",
            "#............................#",
            "##############################"
        ]
    })", "BikeField")};
    return MAPS;
}

MapPosition at(int x, int y){
    return MapPosition{"BikeField", {x, y}};
}

std::string log_text(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n  " + line;
    }
    return text;
}

//  `taps` taps west on the Bicycle, `period` apart, start to start.
SimState tap_chain(int taps, Milliseconds period){
    SimScript script;
    script.on_bike = true;
    OverworldSim sim(field(), at(28, 1), Direction::WEST, script);
    for (int c = 0; c < taps; c++){
        sim.tap(Direction::WEST, BIKE_TAP, period - BIKE_TAP);
    }
    return sim.state();
}

//  Console B, 2026-11-09: a chain of 40ms taps one per 100ms moved a tile
//  a tap; one per 60ms dropped one tap in 12. The walker taps one per
//  BIKE_TAP_PERIOD.
UnitTestResult test_tap_chains(){
    const SimState fast = tap_chain(25, Milliseconds(60));
    if (fast.bike_taps_dropped != 2 || fast.steps != 23){
        return std::format("25 taps 60ms apart: {} dropped, {} tiles; expected 2 and 23.", fast.bike_taps_dropped, fast.steps);
    }
    for (const Milliseconds period : {BIKE_TAP_PERIOD_WORKED, BIKE_TAP_PERIOD}){
        const SimState state = tap_chain(25, period);
        if (state.bike_taps_dropped != 0 || state.steps != 25){
            return std::format("25 taps {}ms apart: {} dropped, {} tiles; expected none and 25.", period.count(), state.bike_taps_dropped, state.steps);
        }
    }
    return true;
}

//  An open leg of `tiles` toward `direction`, ending on `end`.
RouteStep open_leg(Direction direction, int tiles, MapPosition end){
    RouteStep step;
    step.direction = direction;
    step.tiles = tiles;
    step.facing_after = direction;
    step.end = std::move(end);
    return step;
}

//  An open leg of 11 tiles west from (20, 2), believed, then a tile north.
//  The player starts 2 tiles further west, on (18, 2), and the rock at
//  (7, 2) stops them a tile past the leg's end. Counted in taps, the
//  blocked tap places them against the rock, and the route walks back.
//  One hold ends against the rock too, taken for the leg's end, and the
//  tile north ends a column off.
SimState walk_off_start(bool count_taps, std::string& failure, std::vector<std::string>& log){
    const std::vector<RouteStep> route{
        open_leg(Direction::WEST, 11, at(9, 2)),
        open_leg(Direction::NORTH, 1, at(9, 1)),
    };
    SimScript script;
    script.on_bike = true;
    OverworldSim sim(field(), at(18, 2), Direction::WEST, script);
    SimWalkerRules rules;
    rules.speed = MoveSpeed::BIKE;
    rules.count_bike_taps = count_taps;
    OverworldSimWalker walker(sim, rules);
    const RoutePlanning planning{.maps = &field(), .costs = default_route_costs(), .start = at(20, 2), .label = "Sim"};
    failure = walker.run(route, planning);
    log = walker.log();
    log.insert(log.end(), sim.state().log.begin(), sim.state().log.end());
    return sim.state();
}

UnitTestResult test_counted_leg_off_start(){
    std::string failure;
    std::vector<std::string> log;
    const SimState held = walk_off_start(false, failure, log);
    if (failure.empty() && held.position == at(9, 1)){
        return "One hold should end off: the sim doesn't reproduce the start's error." + log_text(log);
    }
    const SimState counted = walk_off_start(true, failure, log);
    if (!failure.empty()){
        return "Counted in taps: " + failure + log_text(log);
    }
    if (!(counted.position == at(9, 1))){
        return "Counted in taps, ended on " + to_string(counted.position) + ", not " + to_string(at(9, 1)) + "." + log_text(log);
    }
    if (counted.bike_taps_dropped != 0 || !counted.violations.empty()){
        return "Counted in taps: a tap dropped or a violation." + log_text(log);
    }
    return true;
}

//  SELECT gets on the registered Bicycle only: a reload of a save made
//  before REGISTER loses it.
UnitTestResult test_select_onto_bike(){
    for (const bool registered : {false, true}){
        SimScript script;
        script.bike_registered = registered;
        OverworldSim sim(field(), at(10, 1), Direction::WEST, script);
        BikeSelectCalls calls;
        int presses = 0;
        calls.press_select = [&]{ presses++; sim.use_field(SimField::BIKE_SELECT); };
        calls.riding = [&]{ return sim.state().on_bike; };
        const bool riding = OverworldWalkSteps::select_onto_bike(calls);
        if (riding != registered || presses != (registered ? 1 : 2)){
            return std::format(
                "Registered {}: riding {} after {} press(es).", registered, riding, presses
            );
        }
    }
    return true;
}

//  A SELECT that shows on the screen a read late: the read right after it
//  still has the player on foot. Pressed again then, SELECT got them off
//  (11.01 ride_bike, console B, 2026-11-12 05:42: "not riding after
//  SELECT", then "riding" on foot, and every bike tap after only turned).
UnitTestResult test_select_onto_bike_shown_late(){
    bool on_bike = false;
    bool shown = false;
    int presses = 0;
    BikeSelectCalls calls;
    calls.press_select = [&]{ presses++; on_bike = !on_bike; };
    calls.riding = [&]{
        const bool read = shown;
        shown = on_bike;
        return read;
    };
    const bool riding = OverworldWalkSteps::select_onto_bike(calls);
    if (!riding || !on_bike || presses != 1){
        return std::format("Read riding {}, on the Bicycle {}, after {} press(es).", riding, on_bike, presses);
    }
    return true;
}

//  A door takes the player off the Bicycle: legs after it go on foot,
//  until one where the Bicycle may be ridden gets on again.
UnitTestResult test_speed_after_door(){
    RouteStep outdoors = open_leg(Direction::WEST, 3, at(7, 1));
    RouteStep indoors = outdoors;
    indoors.walk_only = true;
    using OverworldWalkSteps::gets_on_bike;
    if (!gets_on_bike(outdoors, MoveSpeed::BIKE, false) || gets_on_bike(outdoors, MoveSpeed::BIKE, true)
        || gets_on_bike(indoors, MoveSpeed::BIKE, false) || gets_on_bike(outdoors, MoveSpeed::RUN, false)
    ){
        return "Expected to get on again outdoors only, off the Bicycle, on a ride.";
    }
    if (on_foot(MoveSpeed::BIKE) != MoveSpeed::RUN || on_foot(MoveSpeed::WALK) != MoveSpeed::WALK){
        return "Expected a run off the Bicycle.";
    }
    if (!OverworldWalkSteps::counts_bike_taps(outdoors, MoveSpeed::BIKE) || OverworldWalkSteps::counts_bike_taps(outdoors, MoveSpeed::RUN)){
        return "Expected open legs counted in taps on the Bicycle only.";
    }
    return true;
}

//  A floor that repeats every 16-pixel tile, with a pillar on the bottom
//  row, seen from `scroll` pixels east.
ImageRGB32 periodic_floor(size_t scroll){
    ImageRGB32 image(160, 160);
    for (size_t y = 0; y < 160; y++){
        for (size_t x = 0; x < 160; x++){
            const size_t floor_x = x + scroll;
            uint32_t pixel = (floor_x % 16 < 8) ^ (y % 16 < 8) ? 0xff808040 : 0xffa0a060;
            if (y >= 130 && floor_x >= 100 && floor_x < 120){
                pixel = 0xff000000;
            }
            image.pixel(x, y) = pixel;
        }
    }
    return image;
}

//  Rocket Hideout B3F (09.02 run 1): a tap east over a floor that repeats
//  every tile left the top band the same, so the tap read as a turn.
UnitTestResult test_scroll_over_periodic_floor(){
    const ImageFloatBox game(0, 0, 1, 1);
    const ImageRGB32 before = periodic_floor(0);
    const double stepped = tap_scroll_change(before, periodic_floor(16), game);
    if (stepped <= WALL_TAP_MOVED_RMSD){
        return std::format("Expected a one-tile scroll to read as a step: change {}.", stepped);
    }
    ImageRGB32 turned = periodic_floor(0);
    for (size_t y = 64; y < 96; y++){
        for (size_t x = 64; x < 96; x++){
            turned.pixel(x, y) = 0xffff0000;
        }
    }
    const double bumped = tap_scroll_change(before, turned, game);
    if (bumped > WALL_TAP_MOVED_RMSD){
        return std::format("Expected the player turning in the middle not to read as a step: change {}.", bumped);
    }
    return true;
}

//  An open leg of 11 tiles west on the Bicycle from (20, 2), the player
//  facing south, found off the Bicycle as it starts (11.01, console B,
//  2026-11-12 05:42: on foot, every bike tap only turned the player,
//  "blocked after 0 tiles", and the route replanned 9 times). The tap
//  that didn't move finds them on foot, SELECT gets them on, and the leg
//  is tapped out from there.
UnitTestResult test_leg_starts_off_bike(){
    SimScript script;
    script.on_bike = true;
    script.bike_registered = true;
    script.off_bike_at_legs = {1};
    OverworldSim sim(field(), at(20, 2), Direction::SOUTH, script);
    SimWalkerRules rules;
    rules.speed = MoveSpeed::BIKE;
    OverworldSimWalker walker(sim, rules);
    const RoutePlanning planning{.maps = &field(), .costs = default_route_costs(), .start = at(20, 2), .label = "Sim"};
    const std::string failure = walker.run({open_leg(Direction::WEST, 11, at(9, 2))}, planning);
    std::vector<std::string> log = walker.log();
    log.insert(log.end(), sim.state().log.begin(), sim.state().log.end());
    if (!failure.empty()){
        return failure + log_text(log);
    }
    if (!(sim.state().position == at(9, 2)) || !sim.state().on_bike){
        return std::format("Ended on {}, riding {}; expected {}, riding.", to_string(sim.state().position), sim.state().on_bike, to_string(at(9, 2))) + log_text(log);
    }
    if (sim.state().bike_legs != 1){
        return std::format("{} legs counted in taps; expected the one, not replanned.", sim.state().bike_legs) + log_text(log);
    }
    return true;
}

const BikeCase OVERWORLD_BIKE_CASES[] = {
    {.input = {"a tap over a floor that repeats every tile", test_scroll_over_periodic_floor}, .expectation = {"scroll seen at the bottom edge; a turn is not"}},
    {.input = {"taps a tile each, one in 12 dropped 60ms apart", test_tap_chains}, .expectation = {"23 of 25 tiles at 60ms; 25 at 100 and 120ms"}},
    {.input = {"an open leg of 11 from a start 2 off, counted in taps", test_counted_leg_off_start}, .expectation = {"found against the rock and walked back"}},
    {.input = {"SELECT onto the registered Bicycle", test_select_onto_bike}, .expectation = {"one press registered; two and off unregistered"}},
    {.input = {"SELECT onto the Bicycle shown a read late", test_select_onto_bike_shown_late}, .expectation = {"read again, not pressed off"}},
    {.input = {"on foot after a door, on again outdoors", test_speed_after_door}, .expectation = {"gets_on_bike(), on_foot()"}},
    {.input = {"a leg on the Bicycle that starts off it", test_leg_starts_off_bike}, .expectation = {"SELECT on at the tap that didn't move, the leg walked"}},
};

} // namespace


void add_tests_OverworldBike(UnitTestDatabase& database){
    for (const BikeCase& item : OVERWORLD_BIKE_CASES){
        database.add<Test_OverworldBike>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
