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
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Simulation/PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_RecoveryWalk_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace{
using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::TilePos;

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
    virtual bool run_into_door(Direction, bool, Milliseconds = Milliseconds(12000), Milliseconds = Milliseconds(0)) override{ throw GridSimFailure{"unexpected door"}; }
    virtual std::optional<size_t> tap_to_banner(Direction, const std::string&, size_t) override{ throw GridSimFailure{"unexpected banner"}; }
    virtual void settle(Button, const std::string&, bool) override{ throw GridSimFailure{"unexpected scene"}; }
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
        step.legs = {
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

const Pokemon::StoryStep& story_step(const std::string& row, const std::string& name){
    for (const Pokemon::StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const Pokemon::StoryStep& step : checkpoint.steps){
            if (checkpoint.id == row && step.name == name){
                return step;
            }
        }
    }
    throw GridSimFailure{"no step " + name + " in " + row};
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
            for (const char* name : {"to_roarks_corner", "beside_roarks_rock"}){
                const Pokemon::StoryStep& step = story_step("01.03", name);
                failure = walks.legs(state, step).failure;
                const auto end = story_to_local(world, "OreburghMine", nearest_tile(sim.player().pos));
                logger.log(std::format("{} ended at {}.", name, to_string(sim.player().pos)));
                const std::vector<TilePos> tiles = Pokemon::tiles_of(*step.pose_after);
                const bool on_tile = end && std::find(tiles.begin(), tiles.end(), *end) != tiles.end();
                if (failure.empty() && (!on_tile || Pokemon::facing_of(*step.pose_after) != sim.player().facing)){
                    failure = std::format("{} ended at {} facing {}, not on {}.", name, to_string(sim.player().pos),
                        direction_name(sim.player().facing), Pokemon::to_string(*step.pose_after));
                }
                if (!failure.empty()){
                    break;
                }
                state.pose = *step.pose_after;
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
};

//  The Oreburgh Mine's warps, from the decomp's warp events (Platinum's
//  res/field/maps/*/events.json; BDSP keeps DP's layout): the city's five
//  warp tiles lead to the hall's three 2-1-2, the hall's back a column
//  each, and the coal room's warp to the hall's column 12.
struct WarpLink{
    const char* from_map;
    TilePos from;
    const char* to_map;
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

//  Puts the player on `map`'s `start` and walks north to a warp, then puts
//  them where it lands. Returns where, on which map.
std::pair<std::string, TilePos> warp_north(GridSim& sim, const std::string& map, const TilePos& start){
    const StoryWorld& from = *story_plan_world(map, map);
    sim.place({.world = &from}, story_to_world(from, map, start), Direction::NORTH);
    if (sim.hold(Direction::NORTH, 20000ms, SimPace::WALK).stop != SimStop::WARP){
        throw GridSimFailure{std::format("no warp north of {} on {}", Pokemon::to_string(start), map)};
    }
    const std::optional<TilePos> warp = story_to_local(from, map, *sim.player().warped);
    for (const WarpLink& link : MINE_WARPS){
        if (link.from_map == map && warp == link.from){
            const StoryWorld& to = *story_plan_world(link.to_map, link.to_map);
            sim.place({.world = &to, .trainers = story_trainers(to)}, story_to_world(to, link.to_map, link.to), Direction::NORTH);
            return {link.to_map, link.to};
        }
    }
    throw GridSimFailure{std::format("no warp known at {} on {}", warp ? Pokemon::to_string(*warp) : "?", map)};
}

//  01.03 out of the mine as the table had it before lab-B 2026-11-08: from
//  the city's (45, 59) north against the mine's sign (45, 56), a route to
//  below the Center's door (47, 21), a tap south, the walls east and north
//  of (48, 22), and a tap west.
std::pair<Pokemon::Pose, std::vector<Pokemon::StoryStep>> old_steps_out_of_the_mine(){
    const auto step = [](const char* name, std::vector<Pokemon::StoryLeg> legs, const char* variant, std::optional<TilePos> end){
        Pokemon::StoryStep out;
        out.kind = Pokemon::StoryStepKind::LEGS;
        out.name = name;
        out.legs = std::move(legs);
        out.variant = variant;
        if (end){
            out.pose_after = Pokemon::tile_pose("OreburghCity", {*end}, Direction::NORTH);
        }
        return out;
    };
    Pokemon::StoryStep route;
    route.kind = Pokemon::StoryStepKind::ROUTE;
    route.name = "below_the_centers_door_again";
    route.target = Pokemon::tile_pose("OreburghCity", {TilePos{47, 21}}, Direction::NORTH);
    route.variant = "town";
    return {Pokemon::tile_pose("OreburghCity", {TilePos{45, 59}, TilePos{45, 58}}, Direction::NORTH), {
        step("up_to_the_mines_sign", {{.direction = Direction::NORTH, .tiles = 3}}, "to_wall", TilePos{45, 57}),
        route,
        step("south_of_the_centers_door", {{.direction = Direction::SOUTH, .tiles = 1}}, "", std::nullopt),
        step("against_the_walls_east_of_the_door", {{.direction = Direction::EAST, .tiles = 3}, {.direction = Direction::NORTH, .tiles = 1}},
            "to_wall", TilePos{48, 21}),
        step("below_the_centers_door_lined_up", {{.direction = Direction::WEST, .tiles = 1}}, "", TilePos{47, 21}),
    }};
}
//  The table's 01.03 steps out of the mine, from where it has us on the
//  city to below the Center's door.
std::pair<Pokemon::Pose, std::vector<Pokemon::StoryStep>> table_steps_out_of_the_mine(){
    std::vector<Pokemon::StoryStep> steps;
    for (const Pokemon::StoryCheckpoint& checkpoint : story_checkpoints()){
        bool on = false;
        for (const Pokemon::StoryStep& step : checkpoint.steps){
            on = checkpoint.id == "01.03" && (on || step.name == "off_the_mines_warp_tiles") && step.name != "heal_oreburgh";
            if (on){
                steps.push_back(step);
            }
        }
    }
    return {*story_step("01.03", "out_of_the_mine").pose_after, steps};
}

//  Why `steps`, walked on `sim` from where the player stands, the table
//  having them on `pose`, with open legs overshooting, did not end on
//  (47, 21) with the Center's door north; "" if they did.
std::string center_miss(GridSim& sim, const std::pair<Pokemon::Pose, std::vector<Pokemon::StoryStep>>& steps, Logger& logger){
    std::vector<std::string> notes;
    OvershootingRouteIO route(sim, notes, {});
    WallStepIO io(route, sim);
    StoryWalkSteps walks(io);
    walks.begin_attempt("01.03");
    Pokemon::RunState state{.pose = steps.first, .flags = {}};
    std::string ends;
    try{
        for (const Pokemon::StoryStep& step : steps.second){
            const Pokemon::Outcome outcome = step.kind == Pokemon::StoryStepKind::ROUTE ? walks.route(state, step) : walks.legs(state, step);
            ends += std::format(" {} {}", step.name, to_string(sim.player().pos));
            if (!outcome.succeeded()){
                return step.name + ": " + Pokemon::to_string(outcome);
            }
            state.pose = step.pose_after ? *step.pose_after : outcome.pose ? *outcome.pose : state.pose;
        }
    }catch (const GridSimFailure& error){
        return error.message;
    }
    logger.log("   " + ends);
    const StoryWorld& city = sim.world();
    if (story_to_local(city, "OreburghCity", nearest_tile(sim.player().pos)) != TilePos{47, 21} || !on_grid(sim.player().pos)){
        return std::format("ended at {}, not on (47, 21).", to_string(sim.player().pos));
    }
    return sim.hold(Direction::NORTH, 3000ms, SimPace::RUN).stop == SimStop::WARP ? "" : "no door north of (47, 21).";
}

//  lab-B 2026-11-08 (09:46, 10:39, 12:03): out of the mine, the walk north
//  to the mine's sign never stopped on it, and the route after went up the
//  wrong column; at 12:04 it ran through the Center's door. The coal room's
//  warp lands us on the hall's column 12, which comes out on the city's
//  (46, 59), east of the sign. Up the hall from there, or a column either
//  side should the walk drift, and with a hold on a tile past the fade, the
//  table's steps must end below the Center's door; the old steps miss from
//  column 12.
struct MineExitInput{
    std::string name;
    int hall_column = 12;
    bool past_the_fade = false;
    bool old_steps = false;
};
struct MineExitExpectation{
    bool below_the_door = true;
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
        std::string miss;
        try{
            const StoryWorld& room = *story_plan_world("OreburghMine", "OreburghMine");
            GridSim sim({.world = &room}, story_to_world(room, "OreburghMine", {15, 2}), Direction::NORTH, true);
            const TilePos hall = warp_north(sim, "OreburghMine", {15, 2}).second;
            const TilePos out = warp_north(sim, "OreburghMineB1F", {m_case.input.hall_column, hall.y}).second;
            if (m_case.input.past_the_fade){
                sim.tap(Direction::NORTH, route_ms_per_tile(Direction::NORTH, MoveSpeed::WALK), false);
            }
            miss = center_miss(sim, m_case.input.old_steps ? old_steps_out_of_the_mine() : table_steps_out_of_the_mine(), logger);
            logger.log(std::format("Coal room to the hall's {}, up column {} to the city's {}: {}", Pokemon::to_string(hall),
                m_case.input.hall_column, Pokemon::to_string(out), miss.empty() ? "below the door" : miss));
        }catch (const GridSimFailure& error){
            miss = error.message;
        }catch (const Exception& error){
            miss = std::string(error.name()) + ": " + error.message();
        }
        if (miss.empty() != m_case.expectation.below_the_door){
            return miss.empty() ? "Reached below the door: the test no longer shows the console's failure." : miss;
        }
        return true;
    }
private:
    MineExitCase m_case;
};

const MineExitCase MINE_EXIT_CASES[] = {
    {.input = {.name = "old steps from the hall's column 12", .hall_column = 12, .old_steps = true}, .expectation = {.below_the_door = false}},
    {.input = {.name = "old steps from the hall's column 13", .hall_column = 13, .old_steps = true}, .expectation = {.below_the_door = false}},
    {.input = {.name = "from the hall's column 12", .hall_column = 12}, .expectation = {}},
    {.input = {.name = "from the hall's column 11", .hall_column = 11}, .expectation = {}},
    {.input = {.name = "from the hall's column 13", .hall_column = 13}, .expectation = {}},
    {.input = {.name = "from the hall's column 12, a tile past the fade", .hall_column = 12, .past_the_fade = true}, .expectation = {}},
    {.input = {.name = "from the hall's column 11, a tile past the fade", .hall_column = 11, .past_the_fade = true}, .expectation = {}},
    {.input = {.name = "from the hall's column 13, a tile past the fade", .hall_column = 13, .past_the_fade = true}, .expectation = {}},
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
        {.input = {.name = "first input may belong to previous leg", .taps = {1}}, .expectation = {.failure = "encounter_origin_unknown"}},
        {.input = {.name = "late encounter origin unknown", .taps = {5}, .origin = WalkOrigin::MAY_PRECEDE_LEG}, .expectation = {.failure = "encounter_origin_unknown"}},
    };
    for (const RecoveryCase& scenario : CASES){
        database.add<Test_RecoveryWalk>(scenario);
    }
    for (const CornerCase& scenario : CORNER_CASES){
        database.add<Test_RoarksCorner>(scenario);
    }
    for (const MineExitCase& scenario : MINE_EXIT_CASES){
        database.add<Test_MineExit>(scenario);
    }
}
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
