/*  BDSP Recovery Walk Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  November 5 mine report: EAST 14 moved 4 beside Machop on row 15.
 *  The exact deployed revision/run ID is unavailable. BEFORE/AFTER events
 *  and two interrupted advances are explicit diagnostics, not an inferred
 *  physical cause. Run the real story step and route walker on GridSim.
 */
#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "Simulation/PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_RecoveryWalk_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace detail;
namespace{
using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::TilePos;
using Pokemon::GridPose;
using Pokemon::story_arg;

//  A wild battle in the recovery walk itself, on its first input of the
//  kind: one the console cannot tell from a battle before that input began
//  (WalkOrigin::MAY_PRECEDE_LEG).
enum class RecoveryBattle{ NONE, FIRST_TAP, FIRST_HOLD };

class InterruptingRouteIO : public GridSimRouteIO{
public:
    InterruptingRouteIO(GridSim& sim, std::vector<std::string>& notes,
        std::vector<SimWildTap> taps, size_t interrupted_holds, RecoveryBattle recovery_battle = RecoveryBattle::NONE)
        : GridSimRouteIO(sim, notes, std::move(taps)), m_notes(notes), m_left(interrupted_holds), m_recovery_battle(recovery_battle)
    {}
    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override{
        if (m_left > 0){
            m_left--;
            return {.stop = WalkStop::LIMIT, .walked = 400ms, .wild_battles = 1};
        }
        if (take_recovery_battle(RecoveryBattle::FIRST_HOLD, walk.label)){
            return {.stop = WalkStop::LIMIT, .walked = 400ms, .wild_battles = 1, .origin = WalkOrigin::MAY_PRECEDE_LEG};
        }
        return GridSimRouteIO::walk_to_wall(walk);
    }
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string& label) override{
        if (take_recovery_battle(RecoveryBattle::FIRST_TAP, label)){
            WalkResult result = GridSimRouteIO::walk_taps(direction, tiles - 1, label);
            result.wild_battles++;
            result.moved_chunks++;
            result.origin = WalkOrigin::MAY_PRECEDE_LEG;
            return result;
        }
        return GridSimRouteIO::walk_taps(direction, tiles, label);
    }
private:
    bool take_recovery_battle(RecoveryBattle kind, const std::string& label){
        if (m_recovery_battle != kind || label.find("recovery") == std::string::npos){
            return false;
        }
        m_recovery_battle = RecoveryBattle::NONE;
        m_notes.push_back("[Sim] a wild battle on the first input of " + label);
        return true;
    }

    std::vector<std::string>& m_notes;
    size_t m_left;
    RecoveryBattle m_recovery_battle;
};

//  Only walking is needed for this real LEGS step. Unexpected story actions
//  fail the test. The actual RouteWalker executes through the GridSim RouteIO.
class RecoveryStepIO : public StoryStepIO{
public:
    explicit RecoveryStepIO(RouteIO& route) : m_route(route){}
    virtual RouteIO& route() override{ return m_route; }
    virtual void fight(const std::string&) override{ throw GridSimFailure{"unexpected trainer"}; }
    virtual void walk_until_stopped(Direction, size_t, const std::string&) override{ throw GridSimFailure{"unexpected blind hold"}; }
    virtual bool run_until_map(Direction, Milliseconds, const std::string&, const std::string&) override{ throw GridSimFailure{"unexpected banner walk"}; }
    virtual WarpSeen run_into_door(Direction, bool, Milliseconds = Milliseconds(12000), Milliseconds = Milliseconds(0), const std::string& = "") override{ throw GridSimFailure{"unexpected door"}; }
    virtual std::optional<size_t> tap_to_banner(Direction, const std::string&, size_t) override{ throw GridSimFailure{"unexpected banner"}; }
    virtual void settle(Button, const std::string&, bool, const std::string&) override{ throw GridSimFailure{"unexpected scene"}; }
private:
    RouteIO& m_route;
};

//  The coal room's counted walk from the corridor rock (15, 9) to below
//  Roark (18, 29), as 01.03 walked it before its end was clipped to his
//  rock and the wall: nine counted legs, the last one 9 tiles west along
//  an open row, which is what these recovery cases need.
const Pokemon::StoryStep& mine_step(){
    static const Pokemon::StoryStep step = []{
        Pokemon::StoryStep step;
        step.kind = Pokemon::StoryStepKind::LEGS;
        step.name = "below_roark";
        step.action = "open";
        step.args["legs"] = std::vector<Pokemon::StoryLeg>{
            {.direction = Direction::EAST,  .tiles = 1},
            {.direction = Direction::SOUTH, .tiles = 2},
            {.direction = Direction::WEST,  .tiles = 1},
            {.direction = Direction::SOUTH, .tiles = 5},
            {.direction = Direction::EAST,  .tiles = 14},
            {.direction = Direction::SOUTH, .tiles = 10},
            {.direction = Direction::WEST,  .tiles = 2},
            {.direction = Direction::SOUTH, .tiles = 3},
            {.direction = Direction::WEST,  .tiles = 9},
        };
        return step;
    }();
    return step;
}

struct RecoveryInput{
    std::string name;
    std::vector<size_t> taps;
    Direction direction = Direction::SOUTH;
    size_t tiles = 5;
    SimEncounterPhase phase = SimEncounterPhase::BEFORE_MOVEMENT;
    WalkOrigin origin = WalkOrigin::CURRENT_LEG;
    size_t interrupted_holds = 0;
    RecoveryBattle recovery_battle = RecoveryBattle::NONE;
};
struct RecoveryExpectation{
    std::string failure = {};
    TilePos end = {.x = 18, .y = 29};
    Direction facing = Direction::WEST;
};
struct RecoveryCase{
    RecoveryInput input;
    RecoveryExpectation expectation;
};

class Test_RecoveryWalk : public UnitTest{
public:
    explicit Test_RecoveryWalk(RecoveryCase data)
        : UnitTest("PokemonBDSP::AutoStory - Recovery walk: " + data.input.name), m_case(std::move(data))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const StoryWorld* world = story_plan_world("OreburghMine", "OreburghMine");
        if (world == nullptr){
            return "Missing native mine world.";
        }
        GridSim sim({.world = world}, world->to_world("OreburghMine", {15, 9}), Direction::SOUTH, true);
        std::vector<SimWildTap> taps;
        for (size_t tap : m_case.input.taps){
            taps.push_back({.label = "below_roark", .direction = m_case.input.direction,
                .requested_tiles = m_case.input.tiles, .tap = tap, .phase = m_case.input.phase, .origin = m_case.input.origin});
        }
        std::vector<std::string> notes;
        InterruptingRouteIO route(sim, notes, std::move(taps), m_case.input.interrupted_holds, m_case.input.recovery_battle);
        RecoveryStepIO io(route);
        StoryWalkSteps walks(io);
        walks.begin_attempt("01.03");
        Pokemon::RunState state{.pose = Pokemon::tile_pose("OreburghMine", {{15, 9}}, Direction::SOUTH), .flags = {}};
        std::string failure;
        std::optional<Pokemon::Pose> declared;
        try{
            const auto outcome = walks.legs(state, mine_step());
            failure = outcome.failure;
            declared = outcome.pose;
        }catch (const GridSimFailure& error){
            failure = error.message;
        }
        if (!m_case.expectation.failure.empty()){
            return failure.find(m_case.expectation.failure) != std::string::npos ? UnitTestResult(true)
                : UnitTestResult("Expected " + m_case.expectation.failure + ", got " + failure);
        }
        const auto end = story_to_local(*world, "OreburghMine", nearest_tile(sim.player().pos));
        if (!failure.empty() || !end || *end != m_case.expectation.end || !declared
            || Pokemon::facing_of(*declared) != sim.player().facing || sim.player().facing != m_case.expectation.facing){
            return "Expected below Roark (18,29), got " + to_string(sim.player().pos) + ": " + failure;
        }
        return true;
    }
private:
    RecoveryCase m_case;
};

//  A wild battle on below_roark's open leg east 14, on the input's tap: the
//  leg's screen events must place the player a tile or so from where the
//  taps left them, not anywhere along the 14 tiles. The taps go on after
//  the battle, as the console's do: the player ends at the leg's end, a
//  tile short when the battle came before its tap moved. Red until the
//  story's taps read their events: the leg kept all 15 of its tiles.
struct PlacedBattleInput{
    std::string name;
    size_t tap;
    SimEncounterPhase phase;
};
struct PlacedBattleExpectation{
    size_t tiles_in;    //  how far along east 14 the taps left the player
};
struct PlacedBattleCase{
    PlacedBattleInput input;
    PlacedBattleExpectation expectation;
};

//  "4 to 6 tiles in." in a placement line, if any.
std::optional<std::pair<size_t, size_t>> placed_span(const std::vector<std::string>& notes){
    for (const std::string& note : notes){
        const size_t line = note.find("battle on the leg:");
        const size_t in = note.find(" tiles in.");
        if (line == std::string::npos || in == std::string::npos){
            continue;
        }
        const size_t to = note.rfind(" to ", in);
        const size_t from = note.rfind(' ', to - 1);
        return std::pair<size_t, size_t>{std::stoul(note.substr(from + 1, to - from - 1)), std::stoul(note.substr(to + 4, in - to - 4))};
    }
    return std::nullopt;
}

class Test_PlacedLegBattle : public UnitTest{
public:
    explicit Test_PlacedLegBattle(PlacedBattleCase data)
        : UnitTest("PokemonBDSP::AutoStory - Recovery walk: a battle on an open leg's taps, placed by its screen events: " + data.input.name)
        , m_case(std::move(data))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const StoryWorld* world = story_plan_world("OreburghMine", "OreburghMine");
        GridSim sim({.world = world}, world->to_world("OreburghMine", {15, 9}), Direction::SOUTH, true);
        std::vector<std::string> notes;
        InterruptingRouteIO route(sim, notes, {{.label = "below_roark", .direction = Direction::EAST,
            .requested_tiles = 14, .tap = m_case.input.tap, .phase = m_case.input.phase}}, 0, RecoveryBattle::NONE);
        RecoveryStepIO io(route);
        StoryWalkSteps walks(io);
        walks.begin_attempt("01.03");
        Pokemon::RunState state{.pose = Pokemon::tile_pose("OreburghMine", {{15, 9}}, Direction::SOUTH), .flags = {}};
        std::string failure;
        try{
            failure = walks.legs(state, mine_step()).failure;
        }catch (const GridSimFailure& error){
            failure = error.message;
        }
        for (const std::string& note : notes){
            logger.log(note);
        }
        const auto end = story_to_local(*world, "OreburghMine", nearest_tile(sim.player().pos));
        if (!failure.empty() || !end || *end != TilePos{18, 29}){
            return "Expected below Roark (18, 29), got " + to_string(sim.player().pos) + ": " + failure;
        }
        const auto span = placed_span(notes);
        const size_t tiles_in = m_case.expectation.tiles_in;
        if (!span){
            return "The battle was not placed: the leg kept every one of its tiles.";
        }
        if (span->first > tiles_in || tiles_in > span->second || span->second - span->first > 2){
            return std::format("Placed {} to {} tiles in; the taps left the player {} in.", span->first, span->second, tiles_in);
        }
        return true;
    }
private:
    PlacedBattleCase m_case;
};

//  The route walker's open legs as lab-B 2026-11-08 11:55 saw one: a leg of
//  more than a tile that ends on open ground, not at a wall, goes a tile
//  past where it was planned to stop (there, south 5 went 6). The walker is
//  not told. A single tap stays tile-exact, and so do the story's own
//  counted taps.
class OvershootingRouteIO : public GridSimRouteIO{
public:
    OvershootingRouteIO(GridSim& sim, std::vector<std::string>& notes, std::vector<SimWildTap> taps)
        : GridSimRouteIO(sim, notes, std::move(taps)), m_sim(sim), m_notes(notes)
    {}
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string& label) override{
        const WalkResult result = GridSimRouteIO::walk_taps(direction, tiles, label);
        if (tiles > 1 && label.ends_with("(open)") && result.stop == WalkStop::LIMIT && sim_can_step(m_sim, direction)){
            m_sim.tap(direction, route_ms_per_tile(direction, MoveSpeed::WALK), false);
            m_notes.push_back(std::format("[Sim] {}: overshot a tile {} to {}.", label, direction_name(direction), to_string(m_sim.player().pos)));
        }
        return result;
    }
private:
    GridSim& m_sim;
    std::vector<std::string>& m_notes;
};

//  RecoveryStepIO that also walks to walls, as the story simulation does.
class WallStepIO : public RecoveryStepIO{
public:
    WallStepIO(RouteIO& route, GridSim& sim) : RecoveryStepIO(route), m_sim(sim){}
    virtual void walk_until_stopped(Direction direction, size_t tiles, const std::string&) override{
        m_sim.tap(direction, ROUTE_TURN_TAP, false);
        m_sim.hold(direction, ROUTE_START_DELAY + 800ms + route_ms_per_tile(direction) * (int64_t)(2 * tiles), SimPace::WALK);
    }
private:
    GridSim& m_sim;
};

const Pokemon::StoryStep& story_step(std::string_view row, std::string_view name){
    for (const Pokemon::StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const Pokemon::StoryStep& step : checkpoint.steps){
            if (checkpoint.id == row && step.name == name){
                return step;
            }
        }
    }
    throw GridSimFailure{std::format("no step {} in {}", name, row)};
}

//  lab-B 2026-11-08 11:55: a wild battle on 01.03's counted taps across
//  Roark's room re-planned to the end of the interrupted leg, the open tile
//  (15, 16); the re-plan's open run south overshot a row, and the taps after
//  ran a row off, under Roark's rocks. With a wild battle on the input's
//  tap, the table's to_roarks_corner from (15, 9) must end on his corner
//  (20, 28) facing west, and beside_roarks_rock on (20, 29).
struct CornerInput{
    std::string name;
    std::vector<SimWildTap> battle;
};
struct CornerExpectation{
    std::string failure = {};
};
struct CornerCase{
    CornerInput input;
    CornerExpectation expectation;
};
class Test_RoarksCorner : public UnitTest{
public:
    explicit Test_RoarksCorner(CornerCase data)
        : UnitTest("PokemonBDSP::AutoStory - Recovery walk: 01.03 to Roark's corner, " + data.input.name), m_case(std::move(data))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const StoryWorld& world = *story_plan_world("OreburghMine", "OreburghMine");
        GridSim sim({.world = &world}, world.to_world("OreburghMine", {15, 9}), Direction::SOUTH, true);
        std::vector<std::string> notes;
        OvershootingRouteIO route(sim, notes, m_case.input.battle);
        WallStepIO io(route, sim);
        StoryWalkSteps walks(io);
        walks.begin_attempt("01.03");
        Pokemon::RunState state{.pose = Pokemon::tile_pose("OreburghMine", {{15, 9}}, Direction::SOUTH), .flags = {}};
        std::string failure;
        try{
            for (std::string_view name : {"to_roarks_corner", "beside_roarks_rock"}){
                const Pokemon::StoryStep& step = story_step("01.03", name);
                failure = walks.legs(state, step).failure;
                const auto end = story_to_local(world, "OreburghMine", nearest_tile(sim.player().pos));
                logger.log(std::format("{} ended at {}.", name, to_string(sim.player().pos)));
                const std::vector<TilePos>& tiles = step.pose_after->tiles;
                const bool on_tile = end && std::find(tiles.begin(), tiles.end(), *end) != tiles.end();
                if (failure.empty() && (!on_tile || step.pose_after->facing != sim.player().facing)){
                    failure = std::format("{} ended at {} facing {}, not on {}.", name, to_string(sim.player().pos),
                        direction_name(sim.player().facing), Pokemon::to_string(Pokemon::pose_of(*step.pose_after)));
                }
                if (!failure.empty()){
                    break;
                }
                state.pose = Pokemon::pose_of(*step.pose_after);
            }
        }catch (const GridSimFailure& error){
            failure = error.message;
        }
        for (const std::string& note : notes){
            if (note.find("recovery") != std::string::npos || note.find("overshot") != std::string::npos){
                logger.log(note);
            }
        }
        return failure == m_case.expectation.failure ? UnitTestResult(true)
            : UnitTestResult("Expected '" + m_case.expectation.failure + "', got '" + failure + "'");
    }
private:
    CornerCase m_case;
};

const CornerCase CORNER_CASES[] = {
    {.input = {.name = "no battle", .battle = {}}, .expectation = {}},
    {.input = {.name = "battle on south 5 tap 3",
        .battle = {{.label = "to_roarks_corner", .direction = Direction::SOUTH, .requested_tiles = 5, .tap = 3}}}, .expectation = {}},
    {.input = {.name = "battle on south 5 tap 5",
        .battle = {{.label = "to_roarks_corner", .direction = Direction::SOUTH, .requested_tiles = 5, .tap = 5}}}, .expectation = {}},
    {.input = {.name = "battle on east 14 tap 7",
        .battle = {{.label = "to_roarks_corner", .direction = Direction::EAST, .requested_tiles = 14, .tap = 7}}}, .expectation = {}},
    {.input = {.name = "battle on south 10 tap 4",
        .battle = {{.label = "to_roarks_corner", .direction = Direction::SOUTH, .requested_tiles = 10, .tap = 4}}}, .expectation = {}},
    //  On a leg's first tap the battle may have begun before it (MC seed 90037).
    {.input = {.name = "battle on east 14 tap 1",
        .battle = {{.label = "to_roarks_corner", .direction = Direction::EAST, .requested_tiles = 14, .tap = 1}}}, .expectation = {}},
    {.input = {.name = "battle on south 10 tap 1",
        .battle = {{.label = "to_roarks_corner", .direction = Direction::SOUTH, .requested_tiles = 10, .tap = 1}}}, .expectation = {}},
};

//  The Oreburgh Mine's warps, from the decomp's warp events (Platinum's
//  res/field/maps/*/events.json; BDSP keeps DP's layout): the city's five
//  warp tiles lead to the hall's three 2-1-2, the hall's back a column
//  each, and the coal room's warp to the hall's column 12.
struct WarpLink{
    std::string from_map;
    TilePos from;
    std::string to_map;
    TilePos to;
};
const WarpLink MINE_WARPS[] = {
    {"OreburghCity", {44, 59}, "OreburghMineB1F", {11, 1}},
    {"OreburghCity", {45, 59}, "OreburghMineB1F", {11, 1}},
    {"OreburghCity", {46, 59}, "OreburghMineB1F", {12, 1}},
    {"OreburghCity", {47, 59}, "OreburghMineB1F", {13, 1}},
    {"OreburghCity", {48, 59}, "OreburghMineB1F", {13, 1}},
    {"OreburghMineB1F", {11, 1}, "OreburghCity", {45, 59}},
    {"OreburghMineB1F", {12, 1}, "OreburghCity", {46, 59}},
    {"OreburghMineB1F", {13, 1}, "OreburghCity", {47, 59}},
    {"OreburghMine", {15, 1}, "OreburghMineB1F", {12, 21}},
};

//  Oreburgh's Hiker, who wanders (46..48, 25..27) from (47, 26) on the
//  Diamond decomp's map: across column 46, the walk north to the Center.
SimWanderer oreburgh_hiker(uint64_t seed, Milliseconds start){
    const StoryWorld& city = *story_plan_world("OreburghCity", "OreburghCity");
    const TilePos first = story_to_world(city, "OreburghCity", {46, 25});
    const TilePos last = story_to_world(city, "OreburghCity", {48, 27});
    return {.name = "hiker_0", .box = {first.x, first.y, last.x, last.y}, .pos = story_to_world(city, "OreburghCity", {47, 26}),
        .seed = seed, .step_interval = 2000ms, .time_offset = start};
}

//  Puts the player on `map`'s `start` and walks north to a warp, then puts
//  them where it lands, with `wanderers` about. Returns where, on which map.
std::pair<std::string, TilePos> warp_north(GridSim& sim, const std::string& map, const TilePos& start, std::vector<SimWanderer> wanderers = {}){
    const StoryWorld& from = *story_plan_world(map, map);
    sim.place({.world = &from}, story_to_world(from, map, start), Direction::NORTH);
    if (sim.hold(Direction::NORTH, 20000ms, SimPace::WALK).stop != SimStop::WARP){
        throw GridSimFailure{std::format("no warp north of {} on {}", Pokemon::to_string(start), map)};
    }
    const std::optional<TilePos> warp = story_to_local(from, map, *sim.player().warped);
    for (const WarpLink& link : MINE_WARPS){
        if (link.from_map == map && warp == link.from){
            const StoryWorld& to = *story_plan_world(link.to_map, link.to_map);
            sim.place({.world = &to, .trainers = story_trainers(to), .wanderers = std::move(wanderers)},
                story_to_world(to, link.to_map, link.to), Direction::NORTH);
            return std::pair<std::string, TilePos>{link.to_map, link.to};
        }
    }
    throw GridSimFailure{std::format("no warp known at {} on {}", warp ? Pokemon::to_string(*warp) : "?", map)};
}

//  A LEGS step on Oreburgh City, after which the player stands on one of
//  `end` facing `facing` (nowhere said if `end` is empty).
Pokemon::StoryStep city_legs(std::string_view name, std::vector<Pokemon::StoryLeg> legs, std::string_view variant,
    std::vector<TilePos> end = {}, Direction facing = Direction::NORTH
){
    Pokemon::StoryStep out;
    out.kind = Pokemon::StoryStepKind::LEGS;
    out.name = name;
    out.action = variant;
    out.args["legs"] = std::move(legs);
    out.args["fight_wild"] = false;
    if (!end.empty()){
        out.pose_after = GridPose{.map = "OreburghCity", .tiles = std::move(end), .facing = facing};
    }
    return out;
}
//  A planned route on Oreburgh City to `target`, facing `facing`.
Pokemon::StoryStep city_route(std::string_view name, TilePos target, Direction facing){
    Pokemon::StoryStep out;
    out.kind = Pokemon::StoryStepKind::ROUTE;
    out.name = name;
    out.action = "town";
    const GridPose gp{.map = "OreburghCity", .tiles = {target}, .facing = facing};
    out.args["target"] = gp;
    out.args["fight_wild"] = false;
    out.pose_after = gp;
    return out;
}

using CitySteps = std::pair<Pokemon::Pose, std::vector<Pokemon::StoryStep>>;

//  01.03 out of the mine as the table had it before lab-B 2026-11-08: from
//  the city's (45, 59) north against the mine's sign (45, 56), a route to
//  below the Center's door (47, 21), a tap south, the walls east and north
//  of (48, 22), and a tap west.
CitySteps old_steps_out_of_the_mine(){
    Pokemon::StoryStep route = city_route("below_the_centers_door_again", {47, 21}, Direction::NORTH);
    route.pose_after.reset();
    return {Pokemon::tile_pose("OreburghCity", {TilePos{45, 59}, TilePos{45, 58}}, Direction::NORTH), {
        city_legs("up_to_the_mines_sign", {{.direction = Direction::NORTH, .tiles = 3}}, "to_wall", {{45, 57}}),
        route,
        city_legs("south_of_the_centers_door", {{.direction = Direction::SOUTH, .tiles = 1}}, ""),
        city_legs("against_the_walls_east_of_the_door", {{.direction = Direction::EAST, .tiles = 3}, {.direction = Direction::NORTH, .tiles = 1}},
            "to_wall", {{48, 21}}),
        city_legs("below_the_centers_door_lined_up", {{.direction = Direction::WEST, .tiles = 1}}, "", {{47, 21}}),
    }};
}
//  01.03 out of the mine as the table had it at lab-B 2026-11-08 13:50: pin
//  the column against the walls east of the mine, route to the street west
//  of the door, (44, 22), and line up against the walls east and north of
//  (48, 22). Its route goes round the Hiker's rows west of column 46.
CitySteps previous_steps_out_of_the_mine(){
    return {Pokemon::tile_pose("OreburghCity", {TilePos{46, 59}, TilePos{45, 59}, TilePos{47, 59}, TilePos{46, 58}, TilePos{45, 58}, TilePos{47, 58}},
        Direction::NORTH), {
        city_legs("off_the_mines_warp_tiles", {{.direction = Direction::NORTH, .tiles = 1}}, ""),
        city_legs("east_of_the_mine", {{.direction = Direction::EAST, .tiles = 4}}, "to_wall", {{48, 58}, {49, 57}}, Direction::EAST),
        city_legs("north_of_row_58", {{.direction = Direction::NORTH, .tiles = 1}}, ""),
        city_legs("against_the_walls_east_of_the_mine", {{.direction = Direction::EAST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 1}},
            "to_wall", {{49, 57}}, Direction::SOUTH),
        city_route("west_of_the_centers_door", {44, 22}, Direction::WEST),
        city_legs("against_the_walls_east_of_the_door", {{.direction = Direction::EAST, .tiles = 4}, {.direction = Direction::NORTH, .tiles = 1}},
            "to_wall", {{48, 21}}),
        city_legs("below_the_centers_door_lined_up", {{.direction = Direction::WEST, .tiles = 1}}, "", {{47, 21}}),
    }};
}
//  The table's 01.03 steps out of the mine, from where it has us on the
//  city to below the Center's door.
CitySteps table_steps_out_of_the_mine(){
    std::vector<Pokemon::StoryStep> steps;
    for (const Pokemon::StoryCheckpoint& checkpoint : story_checkpoints()){
        bool on = false;
        for (const Pokemon::StoryStep& step : checkpoint.steps){
            on = checkpoint.id == "01.03" && step.name != "heal_oreburgh" && (on || step.name == "out_of_the_mine");
            if (on && step.name != "out_of_the_mine"){
                steps.push_back(step);
            }
        }
    }
    return {Pokemon::pose_of(*story_step("01.03", "out_of_the_mine").pose_after), steps};
}

//  OvershootingRouteIO that keeps where each walk and tap left the player.
class TracingRouteIO : public OvershootingRouteIO{
public:
    TracingRouteIO(GridSim& sim, std::vector<std::string>& notes)
        : OvershootingRouteIO(sim, notes, {}), m_sim(sim)
    {}
    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override{
        const WalkResult result = OvershootingRouteIO::walk_to_wall(walk);
        m_trail.push_back(m_sim.player().pos);
        return result;
    }
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string& label) override{
        const WalkResult result = OvershootingRouteIO::walk_taps(direction, tiles, label);
        m_trail.push_back(m_sim.player().pos);
        return result;
    }
    void mark(){
        m_trail.push_back(m_sim.player().pos);
    }
    const std::vector<SimPos>& trail() const{
        return m_trail;
    }
private:
    GridSim& m_sim;
    std::vector<SimPos> m_trail;
};

//  How `steps` went: why they did not end on (47, 21) with the Center's
//  door north ("" if they did), and whether the walk stood west of column
//  46 on the way.
struct CenterWalk{
    std::string miss;
    bool west_of_column_46 = false;
};
bool went_west_of_column_46(const StoryWorld& city, const std::vector<SimPos>& trail){
    return std::any_of(trail.begin(), trail.end(), [&city](const SimPos& pos){
        const std::optional<TilePos> tile = story_to_local(city, "OreburghCity", nearest_tile(pos));
        return tile && tile->x < 46;
    });
}
std::string walk_city_steps(StoryWalkSteps& walks, TracingRouteIO& route, const CitySteps& steps, Logger& logger){
    Pokemon::RunState state{.pose = steps.first, .flags = {}};
    std::string ends;
    for (const Pokemon::StoryStep& step : steps.second){
        const Pokemon::Outcome outcome = step.kind == Pokemon::StoryStepKind::ROUTE ? walks.route(state, step) : walks.legs(state, step);
        route.mark();
        ends += std::format(" {} {}", step.name, to_string(route.trail().back()));
        if (!outcome.succeeded()){
            return step.name + ": " + Pokemon::to_string(outcome);
        }
        state.pose = step.pose_after ? Pokemon::pose_of(*step.pose_after) : outcome.pose ? *outcome.pose : state.pose;
    }
    logger.log("   " + ends);
    return "";
}
//  Walks `steps` on `sim` from where the player stands, the table having
//  them on `steps.first`, with open legs overshooting.
CenterWalk center_walk(GridSim& sim, const CitySteps& steps, Logger& logger){
    std::vector<std::string> notes;
    TracingRouteIO route(sim, notes);
    WallStepIO io(route, sim);
    StoryWalkSteps walks(io);
    walks.begin_attempt("01.03");
    CenterWalk out;
    try{
        out.miss = walk_city_steps(walks, route, steps, logger);
    }catch (const GridSimFailure& error){
        out.miss = error.message;
    }catch (const Exception& error){
        out.miss = std::string(error.name()) + ": " + error.message();
    }
    for (const std::string& note : notes){
        if (note.find("waiting") != std::string::npos || note.find("tiles to the wall") != std::string::npos){
            logger.log("    " + note);
        }
    }
    const StoryWorld& city = sim.world();
    out.west_of_column_46 = went_west_of_column_46(city, route.trail());
    if (!out.miss.empty()){
        return out;
    }
    if (story_to_local(city, "OreburghCity", nearest_tile(sim.player().pos)) != TilePos{47, 21} || !on_grid(sim.player().pos)){
        out.miss = std::format("ended at {}, not on (47, 21).", to_string(sim.player().pos));
    }else if (sim.hold(Direction::NORTH, 3000ms, SimPace::RUN).stop != SimStop::WARP){
        out.miss = "no door north of (47, 21).";
    }
    return out;
}

//  lab-B 2026-11-08 (09:46, 10:39, 12:03): out of the mine, the walk north
//  to the mine's sign never stopped on it, and the route after went up the
//  wrong column; at 12:04 it ran through the Center's door. The coal room's
//  warp lands us on the hall's column 12, which comes out on the city's
//  (46, 59), east of the sign. At 13:50 the steps that pinned the column
//  east of the mine went astray too: the hold east from (48, 57) moved 0
//  where the map has (49, 57) open. The table now runs straight north up
//  column 46 to the wall at (46, 20), waiting for Oreburgh's Hiker when he
//  wanders into the column, and taps east below the door. From the hall's
//  column 12, with or without a tile past the fade, and with the Hiker at
//  any seed and start time, its steps must end below the door, (47, 21),
//  and walk in, never west of column 46. From the hall's other columns the
//  count north is far off the map's and the walk must fail.
enum class MineExitSteps{ OLD, PREVIOUS, TABLE };
struct MineExitInput{
    std::string name;
    int hall_column = 12;
    bool past_the_fade = false;
    MineExitSteps steps = MineExitSteps::TABLE;
    //  Every seed and start time of the Hiker's, not just the first.
    bool every_hiker = false;
};
struct MineExitExpectation{
    bool below_the_door = true;
    std::optional<bool> west_of_column_46 = false;
};
struct MineExitCase{
    MineExitInput input;
    MineExitExpectation expectation;
};
class Test_MineExit : public UnitTest{
public:
    explicit Test_MineExit(MineExitCase data)
        : UnitTest("PokemonBDSP::AutoStory - Recovery walk: 01.03 out of the mine on the decomp's warps, " + data.input.name)
        , m_case(std::move(data))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::vector<uint64_t> seeds = m_case.input.every_hiker ? std::vector<uint64_t>{0, 1, 2, 7, 42, 100, 999, 1337} : std::vector<uint64_t>{0};
        const std::vector<Milliseconds> starts = m_case.input.every_hiker
            ? std::vector<Milliseconds>{0ms, 500ms, 1200ms, 2000ms, 3500ms, 5000ms} : std::vector<Milliseconds>{0ms};
        for (uint64_t seed : seeds){
            for (Milliseconds start : starts){
                const std::string failure = run_once(oreburgh_hiker(seed, start), logger);
                if (!failure.empty()){
                    return std::format("the Hiker's seed {}, {} ms in: {}", seed, start.count(), failure);
                }
            }
        }
        return true;
    }
private:
    CitySteps steps() const{
        switch (m_case.input.steps){
        case MineExitSteps::OLD:
            return old_steps_out_of_the_mine();
        case MineExitSteps::PREVIOUS:
            return previous_steps_out_of_the_mine();
        case MineExitSteps::TABLE:
            break;
        }
        return table_steps_out_of_the_mine();
    }
    std::string run_once(const SimWanderer& hiker, Logger& logger) const{
        CenterWalk walk;
        try{
            const StoryWorld& room = *story_plan_world("OreburghMine", "OreburghMine");
            GridSim sim({.world = &room}, story_to_world(room, "OreburghMine", {15, 2}), Direction::NORTH, true);
            const TilePos hall = warp_north(sim, "OreburghMine", {15, 2}).second;
            const TilePos out = warp_north(sim, "OreburghMineB1F", {m_case.input.hall_column, hall.y}, {hiker}).second;
            if (m_case.input.past_the_fade){
                sim.tap(Direction::NORTH, route_ms_per_tile(Direction::NORTH, MoveSpeed::WALK), false);
            }
            walk = center_walk(sim, steps(), logger);
            logger.log(std::format("Coal room to the hall's {}, up column {} to the city's {}: {}{}", Pokemon::to_string(hall),
                m_case.input.hall_column, Pokemon::to_string(out), walk.miss.empty() ? "below the door" : walk.miss,
                walk.west_of_column_46 ? ", west of column 46 on the way" : ""));
        }catch (const GridSimFailure& error){
            walk.miss = error.message;
        }
        if (walk.miss.empty() != m_case.expectation.below_the_door){
            return walk.miss.empty() ? "Reached below the door: the test no longer shows the console's failure." : walk.miss;
        }
        if (m_case.expectation.west_of_column_46 && walk.west_of_column_46 != *m_case.expectation.west_of_column_46){
            return walk.west_of_column_46 ? "The walk went west of column 46." : "The walk stayed on column 46 and east.";
        }
        return "";
    }

    MineExitCase m_case;
};

const MineExitCase MINE_EXIT_CASES[] = {
    {.input = {.name = "old steps from the hall's column 12", .hall_column = 12, .steps = MineExitSteps::OLD},
        .expectation = {.below_the_door = false, .west_of_column_46 = std::nullopt}},
    {.input = {.name = "old steps from the hall's column 13", .hall_column = 13, .steps = MineExitSteps::OLD},
        .expectation = {.below_the_door = false, .west_of_column_46 = std::nullopt}},
    //  The steps of 13:50 reach the door on the map, round the Hiker's rows
    //  to the west.
    {.input = {.name = "13:50's steps from the hall's column 12", .hall_column = 12, .steps = MineExitSteps::PREVIOUS},
        .expectation = {.below_the_door = true, .west_of_column_46 = true}},
    {.input = {.name = "from the hall's column 12", .hall_column = 12}, .expectation = {}},
    {.input = {.name = "from the hall's column 12, a tile past the fade", .hall_column = 12, .past_the_fade = true}, .expectation = {}},
    {.input = {.name = "from the hall's column 12, the Hiker at every seed and start time", .hall_column = 12, .every_hiker = true},
        .expectation = {}},
    {.input = {.name = "from the hall's column 12, a tile past the fade, the Hiker at every seed and start time", .hall_column = 12,
        .past_the_fade = true, .every_hiker = true}, .expectation = {}},
    //  Off column 46 the run north is not the map's 37 or 38 tiles: up
    //  column 45 it stops at the mine's sign, up column 47 it goes through
    //  the Center's door.
    {.input = {.name = "from the hall's column 11", .hall_column = 11},
        .expectation = {.below_the_door = false, .west_of_column_46 = std::nullopt}},
    {.input = {.name = "from the hall's column 13", .hall_column = 13},
        .expectation = {.below_the_door = false, .west_of_column_46 = std::nullopt}},
};

//  A wild battle after `tiles` tiles of the first walk north longer than
//  that, tapped or run until a banner (none if 0); the walk goes on after
//  it, as walk_tiles_dpad() and walk_until_dpad() go on after running.
class ColumnBattle{
public:
    ColumnBattle(std::vector<std::string>& notes, size_t tiles) : m_notes(notes), m_tiles(tiles){}
    size_t tiles() const{ return m_tiles; }
    //  Whether the battle comes after `tile` tiles of a walk of `tiles` `direction`.
    bool after(Direction direction, size_t tiles, size_t tile){
        if (m_taken || m_tiles == 0 || direction != Direction::NORTH || tiles <= m_tiles || tile != m_tiles){
            return false;
        }
        m_taken = true;
        m_notes.push_back(std::format("[Sim] a wild battle after {} tiles north.", tile));
        return true;
    }
private:
    std::vector<std::string>& m_notes;
    size_t m_tiles;
    bool m_taken = false;
};

class ColumnBattleRouteIO : public GridSimRouteIO{
public:
    ColumnBattleRouteIO(GridSim& sim, std::vector<std::string>& notes, ColumnBattle& battle)
        : GridSimRouteIO(sim, notes), m_battle(battle)
    {}
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string& label) override{
        if (!m_battle.after(direction, tiles, m_battle.tiles())){
            return GridSimRouteIO::walk_taps(direction, tiles, label);
        }
        WalkResult result = GridSimRouteIO::walk_taps(direction, m_battle.tiles(), label);
        const WalkResult rest = GridSimRouteIO::walk_taps(direction, tiles - m_battle.tiles(), label);
        result.stop = rest.stop;
        result.walked += rest.walked;
        result.moved_chunks += rest.moved_chunks;
        result.wild_battles++;
        return result;
    }
private:
    ColumnBattle& m_battle;
};

//  RecoveryStepIO that also runs until a map's banner: it reads
//  SIM_BANNER_TILES into the map, as the story simulation's does, or where
//  something stops the run on it.
class BannerStepIO : public RecoveryStepIO{
public:
    static constexpr size_t SIM_BANNER_TILES = 3;
    BannerStepIO(RouteIO& route, GridSim& sim, ColumnBattle& battle) : RecoveryStepIO(route), m_sim(sim), m_battle(battle){}
    virtual bool run_until_map(Direction direction, Milliseconds limit, const std::string& map, const std::string&) override{
        size_t on = 0;
        size_t tiles = 0;
        for (Milliseconds ran = 0ms; ran < limit; ran += ROUTE_RUN_MS_PER_TILE_ACROSS){
            m_battle.after(direction, SIZE_MAX, tiles);
            const SimMove move = m_sim.tap(direction, ROUTE_MS_PER_TILE_ACROSS, false);
            tiles++;
            const bool stopped = move.distance == 0 || move.stop != SimStop::DONE;
            on = story_to_local(m_sim.world(), map, nearest_tile(m_sim.player().pos)) ? on + 1 : 0;
            if (on > 0 && (on >= SIM_BANNER_TILES || stopped)){
                return true;
            }
            if (stopped){
                return false;
            }
        }
        return false;
    }
private:
    GridSim& m_sim;
    ColumnBattle& m_battle;
};

//  01.10's last stretch, north up column 11 from (11, 12) through the grass
//  on (11, 5..11) into Floaroma (lab-B 2026-11-08 18:17:53): a wild battle
//  on "north 7 (open)" to (11, 5) left (11, 5..12), and no plan came, for
//  column 11 has no wall to the north and no other wall brings those tiles
//  together. The table's steps after line 6, with a wild battle the input's
//  tiles up, must end in Floaroma where the row's last walk says.
struct LineSevenInput{
    std::string name;
    size_t battle_after_tiles = 0;
};
struct LineSevenExpectation{
};
struct LineSevenCase{
    LineSevenInput input;
    LineSevenExpectation expectation;
};

UnitTestResult test_line_7_battle_reaches_floaroma(Logger& logger, const LineSevenInput& input){
    const StoryWorld& world = *story_plan_world("Route204North", "FloaromaTown");
    GridSim sim({.world = &world}, story_to_world(world, "Route204North", {11, 12}), Direction::WEST, true);
    std::vector<std::string> notes;
    ColumnBattle battle(notes, input.battle_after_tiles);
    ColumnBattleRouteIO route(sim, notes, battle);
    BannerStepIO io(route, sim, battle);
    StoryWalkSteps walks(io);
    walks.begin_attempt("01.10");
    Pokemon::RunState state{.pose = Pokemon::tile_pose("Route204North", {{11, 12}}, Direction::WEST), .flags = {}};
    std::string failure;
    std::optional<Pokemon::Pose> last;
    try{
        bool after_line_6 = false;
        for (const Pokemon::StoryCheckpoint& checkpoint : story_checkpoints()){
            for (const Pokemon::StoryStep& step : checkpoint.id == "01.10" ? checkpoint.steps : std::vector<Pokemon::StoryStep>{}){
                const bool walks_here = after_line_6 && (step.kind == Pokemon::StoryStepKind::ROUTE || step.kind == Pokemon::StoryStepKind::LEGS);
                after_line_6 |= step.name == "line_6";
                if (!walks_here || !failure.empty()){
                    continue;
                }
                const Pokemon::Outcome outcome = step.kind == Pokemon::StoryStepKind::ROUTE ? walks.route(state, step) : walks.legs(state, step);
                failure = outcome.failure;
                last = step.pose_after ? Pokemon::pose_of(*step.pose_after)
                    : step.args.contains("target") ? Pokemon::pose_of(story_arg<GridPose>(step.args, "target")) : last;
                state.pose = outcome.pose ? *outcome.pose : *last;
                logger.log(std::format("01.10 {} ended at {}.", step.name, to_string(sim.player().pos)));
            }
        }
    }catch (const GridSimFailure& error){
        failure = error.message;
    }
    for (const std::string& note : notes){
        logger.log(note);
    }
    if (!failure.empty() || !last){
        return "01.10 after line 6: " + (failure.empty() ? std::string("no walk") : failure);
    }
    const std::vector<TilePos> tiles = Pokemon::tiles_of(*last);
    const auto end = story_to_local(world, last->map, nearest_tile(sim.player().pos));
    if (!end || std::find(tiles.begin(), tiles.end(), *end) == tiles.end()){
        return std::format("01.10 ended at {}, not on {}.", to_string(sim.player().pos), Pokemon::to_string(*last));
    }
    if (last->map != "FloaromaTown"){
        return "01.10 ends on " + last->map + ", not in Floaroma.";
    }
    return true;
}

class Test_LineSevenBattle : public UnitTest{
public:
    explicit Test_LineSevenBattle(LineSevenCase data)
        : UnitTest("PokemonBDSP::AutoStory - Recovery walk: 01.10 into Floaroma, " + data.input.name), m_case(std::move(data))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return test_line_7_battle_reaches_floaroma(logger, m_case.input);
    }
private:
    LineSevenCase m_case;
};

const LineSevenCase LINE_SEVEN_CASES[] = {
    {.input = {.name = "no battle", .battle_after_tiles = 0}, .expectation = {}},
    {.input = {.name = "a wild battle 3 tiles up (lab-B, 2026-11-08)", .battle_after_tiles = 3}, .expectation = {}},
    {.input = {.name = "a wild battle 6 tiles up", .battle_after_tiles = 6}, .expectation = {}},
};

} // namespace

void add_tests_AutoStory_RecoveryWalk(UnitTestDatabase& database){
    const RecoveryCase CASES[] = {
        {.input = {.name = "uninterrupted native mine step", .taps = {}}, .expectation = {.failure = ""}},
        {.input = {.name = "last tap interrupted before movement", .taps = {5}}, .expectation = {.failure = ""}},
        {.input = {.name = "last tap completed before battle", .taps = {5}, .phase = SimEncounterPhase::AFTER_MOVEMENT}, .expectation = {.failure = ""}},
        {.input = {.name = "two interrupted advances, row 15", .taps = {4, 5}}, .expectation = {.failure = ""}},
        {.input = {.name = "last west leg recovers position and facing", .taps = {9}, .direction = Direction::WEST, .tiles = 9}, .expectation = {.failure = ""}},
        {.input = {.name = "repeat the interrupted recovery hold", .taps = {5}, .interrupted_holds = 1}, .expectation = {.failure = ""}},
        {.input = {.name = "recovery hold budget exhausted", .taps = {5}, .interrupted_holds = 10}, .expectation = {.failure = "recovery budget"}},
        //  A second battle, in the recovery walk, on its first tap or hold
        //  (MC seeds 717266 and 331438): the walk plans again to the step's end.
        {.input = {.name = "battle on the recovery's first tap", .taps = {5}, .recovery_battle = RecoveryBattle::FIRST_TAP}, .expectation = {.failure = ""}},
        {.input = {.name = "battle on the recovery's first hold", .taps = {5}, .recovery_battle = RecoveryBattle::FIRST_HOLD}, .expectation = {.failure = ""}},
        //  A battle that may have begun before the leg: localized from the
        //  leg's tiles all the same, and re-planned to the step's end.
        {.input = {.name = "first input may belong to previous leg", .taps = {1}}, .expectation = {.failure = ""}},
        {.input = {.name = "late encounter origin unknown", .taps = {5}, .origin = WalkOrigin::MAY_PRECEDE_LEG}, .expectation = {.failure = ""}},
    };
    for (const RecoveryCase& scenario : CASES){
        database.add<Test_RecoveryWalk>(scenario);
    }
    const PlacedBattleCase PLACED_CASES[] = {
        {.input = {.name = "tap 3", .tap = 3, .phase = SimEncounterPhase::AFTER_MOVEMENT}, .expectation = {.tiles_in = 14}},
        {.input = {.name = "tap 7, before it moved", .tap = 7, .phase = SimEncounterPhase::BEFORE_MOVEMENT}, .expectation = {.tiles_in = 13}},
        {.input = {.name = "the last tap", .tap = 14, .phase = SimEncounterPhase::AFTER_MOVEMENT}, .expectation = {.tiles_in = 14}},
    };
    for (const PlacedBattleCase& scenario : PLACED_CASES){
        database.add<Test_PlacedLegBattle>(scenario);
    }
    for (const CornerCase& scenario : CORNER_CASES){
        database.add<Test_RoarksCorner>(scenario);
    }
    for (const MineExitCase& scenario : MINE_EXIT_CASES){
        database.add<Test_MineExit>(scenario);
    }
    for (const LineSevenCase& scenario : LINE_SEVEN_CASES){
        database.add<Test_LineSevenBattle>(scenario);
    }
}
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
