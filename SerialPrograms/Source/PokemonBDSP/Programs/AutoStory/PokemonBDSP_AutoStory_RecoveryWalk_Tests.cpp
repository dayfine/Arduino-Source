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
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Utils/PokemonBDSP_AutoStory_GridSim.h"
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
    virtual bool run_into_door(Direction, bool) override{ throw GridSimFailure{"unexpected door"}; }
    virtual std::optional<size_t> tap_to_banner(Direction, const std::string&, size_t) override{ throw GridSimFailure{"unexpected banner"}; }
    virtual void settle(Button, const std::string&, bool) override{ throw GridSimFailure{"unexpected scene"}; }
private:
    RouteIO& m_route;
};

const Pokemon::StoryStep& mine_step(){
    for (const auto& row : story_checkpoints()){
        for (const auto& step : row.steps){
            if (row.id == "01.03" && step.name == "below_roark"){
                return step;
            }
        }
    }
    throw GridSimFailure{"missing native mine step"};
}

struct RecoveryCase{
    std::string name;
    std::vector<size_t> taps;
    Direction direction = Direction::SOUTH;
    size_t tiles = 5;
    SimEncounterPhase phase = SimEncounterPhase::BEFORE_MOVEMENT;
    WalkOrigin origin = WalkOrigin::CURRENT_LEG;
    size_t interrupted_holds = 0;
    std::string failure;
};

class Test_RecoveryWalk : public UnitTest{
public:
    explicit Test_RecoveryWalk(RecoveryCase data)
        : UnitTest("PokemonBDSP::AutoStory - Recovery walk: " + data.name), m_case(std::move(data))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const StoryWorld* world = story_plan_world("OreburghMine", "OreburghMine");
        if (world == nullptr){
            return "Missing native mine world.";
        }
        GridSim sim({.world = world}, world->to_world("OreburghMine", {15, 10}), Direction::SOUTH, true);
        std::vector<SimWildTap> taps;
        for (size_t tap : m_case.taps){
            taps.push_back({.label = "below_roark", .direction = m_case.direction,
                .requested_tiles = m_case.tiles, .tap = tap, .phase = m_case.phase, .origin = m_case.origin});
        }
        std::vector<std::string> notes;
        InterruptingRouteIO route(sim, notes, std::move(taps), m_case.interrupted_holds);
        RecoveryStepIO io(route);
        StoryWalkSteps walks(io);
        walks.begin_attempt("01.03");
        Pokemon::RunState state{.pose = Pokemon::tile_pose("OreburghMine", {{15, 10}}, Direction::SOUTH)};
        std::string failure;
        std::optional<Pokemon::Pose> declared;
        try{
            const auto outcome = walks.legs(state, mine_step());
            failure = outcome.failure;
            declared = outcome.pose;
        }catch (const GridSimFailure& error){
            failure = error.message;
        }
        if (!m_case.failure.empty()){
            return failure.find(m_case.failure) != std::string::npos ? UnitTestResult(true)
                : UnitTestResult("Expected " + m_case.failure + ", got " + failure);
        }
        const auto end = story_to_local(*world, "OreburghMine", nearest_tile(sim.player().pos));
        if (!failure.empty() || !end || *end != TilePos{18, 29} || !declared
            || Pokemon::facing_of(*declared) != sim.player().facing || sim.player().facing != Direction::WEST){
            return "Expected below Roark (18,29), got " + to_string(sim.player().pos) + ": " + failure;
        }
        return true;
    }
private:
    RecoveryCase m_case;
};
}

void add_tests_AutoStory_RecoveryWalk(UnitTestDatabase& database){
    database.add<Test_RecoveryWalk>(RecoveryCase{.name = "uninterrupted native mine step", .taps = {}});
    database.add<Test_RecoveryWalk>(RecoveryCase{.name = "last tap interrupted before movement", .taps = {5}});
    database.add<Test_RecoveryWalk>(RecoveryCase{.name = "last tap completed before battle", .taps = {5}, .phase = SimEncounterPhase::AFTER_MOVEMENT});
    database.add<Test_RecoveryWalk>(RecoveryCase{.name = "two interrupted advances, row 15", .taps = {4, 5}});
    database.add<Test_RecoveryWalk>(RecoveryCase{.name = "last west leg recovers position and facing", .taps = {9}, .direction = Direction::WEST, .tiles = 9});
    database.add<Test_RecoveryWalk>(RecoveryCase{.name = "repeat the interrupted recovery hold", .taps = {5}, .interrupted_holds = 1});
    database.add<Test_RecoveryWalk>(RecoveryCase{.name = "recovery hold budget exhausted", .taps = {5}, .interrupted_holds = 10, .failure = "recovery budget"});
    database.add<Test_RecoveryWalk>(RecoveryCase{.name = "first input may belong to previous leg", .taps = {1}, .failure = "encounter_origin_unknown"});
    database.add<Test_RecoveryWalk>(RecoveryCase{.name = "late encounter origin unknown", .taps = {5}, .origin = WalkOrigin::MAY_PRECEDE_LEG, .failure = "encounter_origin_unknown"});
}
}
}
}
