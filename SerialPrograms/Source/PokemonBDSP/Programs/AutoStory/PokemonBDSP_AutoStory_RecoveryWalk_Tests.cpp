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

class InterruptingRouteIO : public GridSimRouteIO{
public:
    InterruptingRouteIO(GridSim& sim, std::vector<std::string>& notes,
        std::vector<SimWildTap> taps, size_t interrupted_holds)
        : GridSimRouteIO(sim, notes, std::move(taps)), m_left(interrupted_holds)
    {}
    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override{
        if (m_left > 0){
            m_left--;
            return {.stop = WalkStop::LIMIT, .walked = 400ms, .wild_battles = 1};
        }
        return GridSimRouteIO::walk_to_wall(walk);
    }
private:
    size_t m_left;
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
        InterruptingRouteIO route(sim, notes, std::move(taps), m_case.input.interrupted_holds);
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
        {.input = {.name = "first input may belong to previous leg", .taps = {1}}, .expectation = {.failure = "encounter_origin_unknown"}},
        {.input = {.name = "late encounter origin unknown", .taps = {5}, .origin = WalkOrigin::MAY_PRECEDE_LEG}, .expectation = {.failure = "encounter_origin_unknown"}},
    };
    for (const RecoveryCase& scenario : CASES){
        database.add<Test_RecoveryWalk>(scenario);
    }
    for (const CornerCase& scenario : CORNER_CASES){
        database.add<Test_RoarksCorner>(scenario);
    }
}
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
