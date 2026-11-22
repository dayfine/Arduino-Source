/*  BDSP AutoStory Grid Simulation - Wild Battles on Wall Legs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A wild battle on a leg to a wall, walked by the route walker on the grid
 *  simulation: however the escape ends the battle, the leg must still end at
 *  the wall with the battle counted. And a run into a scene (00.03's into
 *  Sandgem), a wild battle on it or not: the scene stops the run wherever it
 *  starts, and with no scene the run stops at its cap and the step fails.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryPlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryRoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryPlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryCheckpoint.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryStepIO.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryWalkSteps.h"
#include "PokemonBDSP_AutoStoryGridSim.h"
#include "PokemonBDSP_AutoStorySim.h"
#include "PokemonBDSP_AutoStoryWallBattleSim_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace detail;


namespace{

using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::TilePos;


struct WallBattleCase{
    struct Input{
        SimWildBattle battle;
    };
    struct Expectation{
        //  How the simulation notes the battle ended.
        std::string ended;
    };
    std::string name;
    Input input;
    Expectation expectation;
};

StoryWorld make_world(const std::vector<std::string>& rows){
    Pokemon::MapData map;
    map.name = "Test";
    map.tiles = Pokemon::Grid<Pokemon::TileKind>{(int)rows[0].size(), (int)rows.size(), {}};
    for (const std::string& row : rows){
        for (char glyph : row){
            map.tiles.cells.push_back(*Pokemon::tile_from_glyph(glyph));
        }
    }
    StoryWorld world;
    world.add("Test", map, {0, 0});
    return world;
}

//  The corridor's one leg, east to its end wall, walked with `battle` on
//  the way; the walk's notes, or why it did not end at the wall.
struct WallBattleWalk{
    std::vector<std::string> notes;
    std::string error;
};
WallBattleWalk walk_wall_battle(const std::string& leg, const SimWildBattle& battle){
    StoryWorld world = make_world({
        "########################",
        "#......................#",
        "########################",
    });
    const TilePos start{1, 1};
    const TilePos target{22, 1};
    GridSim sim({.world = &world}, start, Direction::EAST, true);
    WallBattleWalk walk;
    GridSimRouteIO sim_io(sim, walk.notes, {}, {{.label = leg, .direction = Direction::EAST, .battle = battle}});
    RouteTrainers trainers{{}, [](const std::string&){}};
    try{
        walk_route(sim_io, world, {start}, trainers, {
            .goal = [&](const TilePos& pos){ return pos == target; },
            .label = "wall leg battle",
            .avoid = {},
            .costs = *story_route_costs("town"),
            .speed = MoveSpeed::RUN,
        });
    }catch (const GridSimFailure& f){
        walk.error = "failed: " + f.message;
        return walk;
    }
    if (nearest_tile(sim.player().pos) != target){
        walk.error = "ended at " + to_string(sim.player().pos) + ", not at the wall.";
    }
    return walk;
}

class Test_WallBattleSim : public UnitTest{
public:
    explicit Test_WallBattleSim(WallBattleCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: a wild battle on a wall leg: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string leg = "wall leg battle: " + to_string(TileLeg{.direction = Direction::EAST, .tiles = 21, .ends_at_wall = true, .held = false});
        const WallBattleWalk walk = walk_wall_battle(leg, m_case.input.battle);
        if (!walk.error.empty()){
            return walk.error;
        }
        const std::string ended = std::format("[Sim] {}: wild battle {}.", leg, m_case.expectation.ended);
        if (std::find(walk.notes.begin(), walk.notes.end(), ended) == walk.notes.end()){
            std::string notes;
            for (const std::string& line : walk.notes){
                notes += "\n    " + line;
            }
            return std::format("expected the note \"{}\"; the walk noted:{}", ended, notes);
        }
        return true;
    }

private:
    WallBattleCase m_case;
};

const WallBattleCase WALL_BATTLE_SIM_CASES[] = {
    {.name = "the first Run works", .input = {.battle = {.run_refusals = 0}}, .expectation = {.ended = "run from"}},
    {.name = "two refusals, then the third Run works", .input = {.battle = {.run_refusals = 2}}, .expectation = {.ended = "run from"}},
    //  console B, BDSP 01.03 run 2026-11-07: Onix's Bind in Oreburgh Mine
    //  refused nine Runs in 60 s and escape_wild_battle() gave up.
    {.name = "bound: every Run refused", .input = {.battle = {.run_refusals = 20}}, .expectation = {.ended = "fought to its end"}},
};


//  The route walker's IO alone, for a ROUTE step: anything else the step
//  asks of the console fails the walk.
class RouteOnlyStepIO : public StoryStepIO{
public:
    explicit RouteOnlyStepIO(GridSimRouteIO& route)
        : m_route(route)
    {}

    virtual RouteIO& route() override{
        return m_route;
    }
    virtual void fight(const std::string& trainer) override{
        unsupported("a fight with " + trainer);
    }
    virtual void walk_until_stopped(Direction, size_t, const std::string& label) override{
        unsupported(label + ": a walk until stopped");
    }
    virtual bool run_until_map(Direction, Milliseconds, const std::string& map, const std::string&) override{
        unsupported("a run onto " + map);
        return false;
    }
    virtual WarpSeen run_into_door(Direction, bool, const Pokemon::DoorApproachPlan&, const std::string&) override{
        unsupported("a run into a door");
        return {};
    }
    virtual std::optional<size_t> tap_to_banner(Direction, const std::string& map, size_t) override{
        unsupported("taps to " + map + "'s banner");
        return std::nullopt;
    }
    virtual bool settle(Button, const std::string& label, SettleKind, const std::string&) override{
        unsupported(label + "'s settle");
        return false;
    }

private:
    void unsupported(const std::string& what){
        m_route.note(what + " is not simulated here.", RouteNote::FAILURE);
    }

    GridSimRouteIO& m_route;
};

//  Dawn's scene on `world`, in its tiles: the coord events of Sandgem that
//  00.03 arms (its trigger column (4, 10..15), decomp).
std::vector<SimTrigger> dawns_triggers(const StoryWorld& world){
    std::vector<SimTrigger> out;
    for (const RowTrigger& armed : story_triggers()){
        if (armed.row != "00.03" || armed.map != "SandgemTown"){
            continue;
        }
        SimTrigger trigger{.name = armed.trigger.name, .tiles = {}};
        for (const TilePos& pos : armed.trigger.tiles){
            trigger.tiles.push_back(story_to_world(world, armed.map, pos));
        }
        out.push_back(std::move(trigger));
    }
    if (out.empty()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "00.03 arms no coord event in Sandgem.");
    }
    return out;
}

//  The table's 00.03 into_dawns_scene.
const BdspStep& into_dawns_scene(){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const BdspStep& step : checkpoint.steps){
            if (checkpoint.id == "00.03" && step.name == "into_dawns_scene" &&
                step.action == "route" && std::get<RouteStepArgs>(step.args).legs.size() == 1){
                return step;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "00.03 has no route into_dawns_scene with a run.");
}

struct SceneRunCase{
    struct Input{
        bool dawn = false;
        bool battle = false;
    };
    struct Expectation{
        bool in_scene = false;
    };
    std::string name;
    Input input;
    Expectation expectation;
};

struct SceneRun{
    std::optional<Pokemon::StoryFailure> failure;
    TilePos end;
    int run_tiles = 0;
    bool dialog = false;
    bool ran_from_battle = false;
    std::string program_failure;
};

SceneRun run_into_dawns_scene(const SceneRunCase::Input& input){
    const BdspStep& step = into_dawns_scene();
    const GridPose start{.map = "TwinleafTown", .tiles = {map_point("TwinleafTown", "house_doorstep")}, .facing = Direction::SOUTH};
    const GridPose& target = std::get<RouteStepArgs>(step.args).target;
    const StoryWorld& world = *story_plan_world(start.map, target.map);
    SimWorld sim_world{.world = &world, .trainers = story_trainers(world), .can_run = true};
    if (input.dawn){
        sim_world.triggers = dawns_triggers(world);
    }
    GridSim sim(sim_world, story_to_world(world, start.map, start.tiles.front()), Direction::SOUTH, true);
    std::vector<std::string> notes;
    std::vector<SimWallEncounter> encounters;
    if (input.battle){
        encounters.push_back({.label = "into dawns scene", .direction = Direction::EAST, .battle = {}});
    }
    GridSimRouteIO route_io(sim, notes, {}, std::move(encounters));
    RouteOnlyStepIO io(route_io);
    StoryWalkSteps walk(io);
    walk.begin_attempt("00.04");
    BdspRunState state{.pose = start};
    SceneRun out;
    try{
        out.failure = walk.route(state, step);
    }catch (const GridSimFailure& e){
        out.program_failure = e.message;
    }
    out.end = nearest_tile(sim.player().pos);
    out.run_tiles = out.end.x - story_to_world(world, target.map, target.tiles.front()).x;
    out.dialog = sim.player().dialog;
    out.ran_from_battle = std::find(notes.begin(), notes.end(), "[Sim] into dawns scene: wild battle run from.") != notes.end();
    return out;
}

std::string scene_run_error(const SceneRunCase& test_case, const SceneRun& run){
    const StoryWorld& world = *story_plan_world("Route201", "SandgemTown");
    const std::optional<TilePos> sandgem = story_to_local(world, "SandgemTown", run.end);
    const int cap = std::get<RouteStepArgs>(into_dawns_scene().args).legs.front().tiles;
    const int most = (int)((route_limit_for_tiles(MoveSpeed::RUN, cap) - ROUTE_START_DELAY) / route_ms_per_tile(Direction::EAST, MoveSpeed::RUN));
    if (!run.program_failure.empty()){
        return "the program failed instead of the step: " + run.program_failure;
    }
    if (test_case.input.battle != run.ran_from_battle){
        return std::format("expected {} wild battle on the run.", test_case.input.battle ? "a" : "no");
    }
    if (run.run_tiles < 0 || run.run_tiles > most){
        return std::format("the run went {} tiles past the route's end, outside its cap of {} ({} at the sim's pace).", run.run_tiles, cap, most);
    }
    const std::string status = run.failure ? std::format("{}", *run.failure) : std::string("ok");
    if (test_case.expectation.in_scene && (run.failure || !run.dialog || !sandgem || sandgem->x != 4)){
        return std::format("\"{}\", ended at {}, not in Dawn's scene on Sandgem's column 4.", status, describe(world, run.end));
    }
    if (!test_case.expectation.in_scene && (!run.failure || run.failure->code != "no_dialogue")){
        return std::format("expected the step to fail as no_dialogue; \"{}\".", status);
    }
    return "";
}

class Test_SceneRunSim : public UnitTest{
public:
    explicit Test_SceneRunSim(SceneRunCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: 00.03's run into Sandgem: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const SceneRun run = run_into_dawns_scene(m_case.input);
        const std::string status = run.failure ? std::format("{}", *run.failure) : std::string("ok");
        logger.log(std::format("{}: \"{}\", ended {} tiles on.", m_case.name, status, run.run_tiles));
        const std::string error = scene_run_error(m_case, run);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    SceneRunCase m_case;
};

const SceneRunCase SCENE_RUN_SIM_CASES[] = {
    {.name = "Dawn's scene stops it mid-run", .input = {.dawn = true, .battle = false}, .expectation = {.in_scene = true}},
    {.name = "Dawn's scene stops it after a wild battle on the run", .input = {.dawn = true, .battle = true}, .expectation = {.in_scene = true}},
    {.name = "no scene: the step fails at the cap", .input = {.dawn = false, .battle = false}, .expectation = {.in_scene = false}},
    {.name = "a wild battle and no scene: the step fails at the cap", .input = {.dawn = false, .battle = true}, .expectation = {.in_scene = false}},
};

} // namespace


void add_tests_AutoStory_WallBattleSim(UnitTestDatabase& database){
    for (const WallBattleCase& item : WALL_BATTLE_SIM_CASES){
        database.add<Test_WallBattleSim>(item);
    }
    for (const SceneRunCase& item : SCENE_RUN_SIM_CASES){
        database.add<Test_SceneRunSim>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
