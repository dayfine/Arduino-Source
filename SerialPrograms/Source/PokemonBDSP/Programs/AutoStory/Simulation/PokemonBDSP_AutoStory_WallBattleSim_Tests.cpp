/*  BDSP AutoStory Grid Simulation - Wild Battles on Wall Legs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A wild battle on a leg to a wall, walked by the route walker on the grid
 *  simulation: however the escape ends the battle, the leg must still end at
 *  the wall with the battle counted. And a run into a scene (00.03's into
 *  Sandgem), a wild battle on it or not: the scene stops the run wherever it
 *  starts, and with no scene the run stops at its cap and the step fails.
 *  And 00.05c's greeter by the Wonder Station, who talks each time we come
 *  near: heard out each time, the walk still arrives. And a talk no battle
 *  follows that outlasts a press-through: heard out, the walk goes round.
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
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_StorySim.h"
#include "PokemonBDSP_AutoStory_WallBattleSim_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace detail;


namespace{

using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::story_arg;
using Pokemon::StoryStep;
using Pokemon::TilePos;


struct WallBattleInput{
    std::string name;
    SimWildBattle battle;
};
struct WallBattleExpectation{
    //  How the simulation notes the battle ended.
    std::string ended;
};
struct WallBattleCase{
    WallBattleInput input;
    WallBattleExpectation expectation;
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
    RouteTrainers trainers{{}, [](const std::string&){ return true; }};
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
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: a wild battle on a wall leg: " + test_case.input.name)
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
    {.input = {.name = "the first Run works", .battle = {.run_refusals = 0}}, .expectation = {.ended = "run from"}},
    {.input = {.name = "two refusals, then the third Run works", .battle = {.run_refusals = 2}}, .expectation = {.ended = "run from"}},
    //  console B, BDSP 01.03 run 2026-11-07: Onix's Bind in Oreburgh Mine
    //  refused nine Runs in 60 s and escape_wild_battle() gave up.
    {.input = {.name = "bound: every Run refused", .battle = {.run_refusals = 20}}, .expectation = {.ended = "fought to its end"}},
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
    virtual bool fight(const std::string& trainer) override{
        unsupported("a fight with " + trainer);
        return false;
    }
    virtual void walk_until_stopped(Direction, size_t, const std::string& label) override{
        unsupported(label + ": a walk until stopped");
    }
    virtual bool run_until_map(Direction, Milliseconds, const std::string& map, const std::string&) override{
        unsupported("a run onto " + map);
        return false;
    }
    virtual WarpSeen run_into_door(Direction, bool, Milliseconds, Milliseconds, const std::string&) override{
        unsupported("a run into a door");
        return {};
    }
    virtual std::optional<size_t> tap_to_banner(Direction, const std::string& map, size_t) override{
        unsupported("taps to " + map + "'s banner");
        return std::nullopt;
    }
    virtual void settle(Button, const std::string& label, SettleKind, const std::string&) override{
        unsupported(label + "'s settle");
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
const StoryStep& into_dawns_scene(){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
            if (checkpoint.id == "00.03" && step.name == "into_dawns_scene" &&
                step.args.contains("target") &&
                step.args.contains("legs") &&
                story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs").size() == 1){
                return step;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "00.03 has no ROUTE into_dawns_scene with a run.");
}

struct SceneRunInput{
    std::string name;
    //  Dawn's scene is armed.
    bool dawn = false;
    //  A wild battle interrupts the run.
    bool battle = false;
};
struct SceneRunExpectation{
    //  The step ends in Dawn's scene; else it fails as "no_dialogue".
    bool in_scene = false;
};
struct SceneRunCase{
    SceneRunInput input;
    SceneRunExpectation expectation;
};

//  How 00.03's into_dawns_scene went: its outcome, where it left the player,
//  how many tiles east of the route's end, whether a dialogue is open,
//  whether a wild battle was run from, and the failure the program would
//  have stopped on instead of failing the step.
struct SceneRun{
    Pokemon::Outcome outcome;
    TilePos end;
    int run_tiles = 0;
    bool dialog = false;
    bool ran_from_battle = false;
    std::string program_failure;
};
//  00.03's into_dawns_scene from the house's doorstep, where the step before
//  it leaves us, at the run's pace (Mom gave the shoes in 00.03): the route
//  to Route 201's east edge, then the run east into Sandgem.
SceneRun run_into_dawns_scene(const SceneRunInput& input){
    const StoryStep& step = into_dawns_scene();
    const Pokemon::Pose start = Pokemon::tile_pose("TwinleafTown", {map_point("TwinleafTown", "house_doorstep")}, Direction::SOUTH);
    const GridPose& target = story_arg<GridPose>(step.args, "target");
    const StoryWorld& world = *story_plan_world(start.map, target.map);
    SimWorld sim_world{.world = &world, .trainers = story_trainers(world), .can_run = true};
    if (input.dawn){
        sim_world.triggers = dawns_triggers(world);
    }
    GridSim sim(sim_world, story_to_world(world, start.map, Pokemon::tiles_of(start).front()), Direction::SOUTH, true);
    std::vector<std::string> notes;
    std::vector<SimWallEncounter> encounters;
    if (input.battle){
        encounters.push_back({.label = "into dawns scene", .direction = Direction::EAST, .battle = {}});
    }
    GridSimRouteIO route_io(sim, notes, {}, std::move(encounters));
    RouteOnlyStepIO io(route_io);
    StoryWalkSteps walk(io);
    //  A row after the shoes' row runs, as 00.03 does once Mom gave them.
    walk.begin_attempt("00.04");
    Pokemon::RunState state{.pose = start, .flags = {}};
    SceneRun out;
    try{
        out.outcome = walk.route(state, step);
    }catch (const GridSimFailure& e){
        out.program_failure = e.message;
    }
    out.end = nearest_tile(sim.player().pos);
    out.run_tiles = out.end.x - story_to_world(world, target.map, target.tiles.front()).x;
    out.dialog = sim.player().dialog;
    out.ran_from_battle = std::find(notes.begin(), notes.end(), "[Sim] into dawns scene: wild battle run from.") != notes.end();
    return out;
}

//  Why `run` is not what `test_case` expects, or "".
std::string scene_run_error(const SceneRunCase& test_case, const SceneRun& run){
    const StoryWorld& world = *story_plan_world("Route201", "SandgemTown");
    const std::optional<TilePos> sandgem = story_to_local(world, "SandgemTown", run.end);
    const int cap = story_arg<std::vector<Pokemon::StoryLeg>>(into_dawns_scene().args, "legs").front().tiles;
    //  The cap is timed at the slowest run; the sim runs faster, this far.
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
    if (test_case.expectation.in_scene && (!run.outcome.succeeded() || !run.dialog || !sandgem || sandgem->x != 4)){
        return std::format("\"{}\", ended at {}, not in Dawn's scene on Sandgem's column 4.", Pokemon::to_string(run.outcome), describe(world, run.end));
    }
    if (!test_case.expectation.in_scene && run.outcome.failure != "no_dialogue"){
        return std::format("expected the step to fail as no_dialogue; \"{}\".", Pokemon::to_string(run.outcome));
    }
    return "";
}

class Test_SceneRunSim : public UnitTest{
public:
    explicit Test_SceneRunSim(SceneRunCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: 00.03's run into Sandgem: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const SceneRun run = run_into_dawns_scene(m_case.input);
        logger.log(std::format("{}: \"{}\", ended {} tiles on.", m_case.input.name, Pokemon::to_string(run.outcome), run.run_tiles));
        const std::string error = scene_run_error(m_case, run);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    SceneRunCase m_case;
};

const SceneRunCase SCENE_RUN_SIM_CASES[] = {
    {.input = {.name = "Dawn's scene stops it mid-run", .dawn = true, .battle = false}, .expectation = {.in_scene = true}},
    {.input = {.name = "Dawn's scene stops it after a wild battle on the run", .dawn = true, .battle = true}, .expectation = {.in_scene = true}},
    {.input = {.name = "no scene: the step fails at the cap", .dawn = false, .battle = false}, .expectation = {.in_scene = false}},
    {.input = {.name = "a wild battle and no scene: the step fails at the cap", .dawn = false, .battle = true}, .expectation = {.in_scene = false}},
};

//  The Wonder Station's greeter in Jubilife's plaza walks up and talks
//  whenever we come near, then walks off: on lab-B (2026-11-12 03:57, 00.05c
//  below_the_company_clown; 00.05c's first walk is now beside_the_president)
//  she stopped one walk three times, at about
//  (44, 28), (43, 30) and (39, 32), and the third failed it as "dialogue the
//  map does not explain". Here a step onto the step's greeter ground starts
//  her dialogue, and so does the next one after each talk, `talks` in all.
struct GreeterInput{
    std::string name;
    size_t talks;
};
struct GreeterExpectation{
    bool arrives;
};
struct GreeterCase{
    GreeterInput input;
    GreeterExpectation expectation;
};

//  Where the walk past `talks` of the greeter's went wrong, or "".
std::string walk_past_the_greeter(Logger& logger, size_t most_talks){
    const StoryStep* step = nullptr;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& s : row.steps){
            step = row.id == "00.05c" && s.name == "beside_the_president" ? &s : step;
        }
    }
    const StoryWorld* world = story_plan_world("JubilifeCity", "JubilifeCity");
    if (step == nullptr || world == nullptr){
        return "00.05c beside_the_president: not in the table, or no JubilifeCity world.";
    }
    const std::vector<TilePos> greeter = story_route_tiles(*world, *step, "greeter");
    const TilePos start = story_to_world(*world, "JubilifeCity", {57, 22});
    const TilePos target = story_to_world(*world, "JubilifeCity", {45, 35});
    GridSim sim({.world = world, .trainers = {}, .triggers = {}}, start, Direction::WEST, true);
    size_t talks = 0;
    const auto arm = [&](){
        sim_add_trigger(sim, {.name = std::format("the greeter {}", talks), .tiles = greeter, .spends = {}});
    };
    arm();
    std::vector<std::string> notes;
    GridSimRouteIO sim_io(sim, notes);
    RouteTrainers trainers{{}, [&](const std::string&){
        sim.end_dialogue();
        talks++;
        if (talks < most_talks){
            arm();
        }
        return false;
    }};
    std::vector<TilePos> believed;
    try{
        believed = walk_route(sim_io, *world, {start}, trainers, {.goal = [&](const TilePos& pos){ return pos == target; },
            .label = "beside the president", .avoid = story_route_tiles(*world, *step, "avoid"), .costs = *story_route_costs("town"),
            .speed = MoveSpeed::RUN, .scene_next = false, .goal_is_trigger = false, .greeter = greeter});
    }catch (const GridSimFailure& e){
        return std::format("failed after {} talk(s): {}", talks, e.message);
    }
    for (const std::string& note : notes){
        logger.log(note);
    }
    const TilePos at = nearest_tile(sim.player().pos);
    const bool arrived = believed == std::vector<TilePos>{target} && at == target && talks == most_talks;
    return arrived ? "" : std::format("ended on {} after {} talk(s), not on {}.", describe(*world, at), talks, describe(*world, target));
}

class Test_GreeterSim : public UnitTest{
public:
    explicit Test_GreeterSim(GreeterCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: 00.05c, the Wonder Station's greeter " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string error = walk_past_the_greeter(logger, m_case.input.talks);
        if (error.empty() != m_case.expectation.arrives){
            return error.empty() ? "arrived" : error;
        }
        return true;
    }

private:
    GreeterCase m_case;
};

const GreeterCase GREETER_SIM_CASES[] = {
    {.input = {.name = "talks once", .talks = 1}, .expectation = {.arrives = true}},
    {.input = {.name = "talks again on each leg near her (lab-B, 2026-11-12 03:57)", .talks = 3}, .expectation = {.arrives = true}},
};

//  A talk no battle followed, long enough to outlast a press-through, from
//  someone who stands on the leg: on lab-B (2026-11-12 04:58, 00.05c
//  below_the_company_clown, the 5th failure there) the Wonder Station's
//  woman stopped "west 1 (to wall)" as "a trainer on the way", 2 s of B left
//  her explanation on screen, and the walk on into her ran out of time.
//  Here a corridor two rows high: her talk starts on (11, 1) and takes
//  `presses` press-throughs to end; with `stands` she stays on (12, 1),
//  which our map has open.
struct TalkerInput{
    std::string name;
    size_t presses;
    bool stands;
};
struct TalkerExpectation{
    bool arrives;
};
struct TalkerCase{
    TalkerInput input;
    TalkerExpectation expectation;
};

//  One press-through of the talk: the `needed`th ends it.
void press_through(GridSim& sim, size_t& presses, size_t needed){
    if (sim.player().dialog && ++presses >= needed){
        sim.end_dialogue();
    }
}

//  Where the walk past the talker went wrong, or "".
std::string walk_past_the_talker(Logger& logger, const TalkerInput& input){
    const std::vector<std::string> open{
        "########################",
        "#......................#",
        "#......................#",
        "########################",
    };
    std::vector<std::string> real = open;
    if (input.stands){
        real[1][12] = '#';
    }
    const StoryWorld map = make_world(open);
    const StoryWorld world = make_world(real);
    const TilePos start{1, 1};
    const TilePos target{22, 1};
    const SimTrigger talk{.name = "the woman", .tiles = {TilePos{11, 1}}};
    GridSim sim({.world = &world, .trainers = {}, .triggers = {talk}}, start, Direction::EAST, true);
    std::vector<std::string> notes;
    GridSimRouteIO sim_io(sim, notes);
    size_t presses = 0;
    RouteTrainers trainers{{}, [&](const std::string&){
        press_through(sim, presses, input.presses);
        return false;
    }};
    std::vector<TilePos> believed;
    try{
        believed = walk_route(sim_io, map, {start}, trainers, {.goal = [&](const TilePos& pos){ return pos == target; },
            .label = "past the talker", .avoid = {}, .costs = *story_route_costs("town"), .speed = MoveSpeed::RUN});
    }catch (const GridSimFailure& e){
        return std::format("failed after {} press-through(s): {}", presses, e.message);
    }
    for (const std::string& note : notes){
        logger.log(note);
    }
    const TilePos at = nearest_tile(sim.player().pos);
    const bool arrived = believed == std::vector<TilePos>{target} && at == target;
    return arrived ? "" : std::format("ended on {} believing {}, not on {}.", to_string(sim.player().pos),
        believed.empty() ? std::string("nothing") : describe(map, believed[0]), describe(map, target));
}

class Test_TalkerSim : public UnitTest{
public:
    explicit Test_TalkerSim(TalkerCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: a talk no battle follows " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string error = walk_past_the_talker(logger, m_case.input);
        if (error.empty() != m_case.expectation.arrives){
            return error.empty() ? "arrived" : error;
        }
        return true;
    }

private:
    TalkerCase m_case;
};

const TalkerCase TALKER_SIM_CASES[] = {
    {.input = {.name = "outlasts a press-through, then she walks off", .presses = 3, .stands = false}, .expectation = {.arrives = true}},
    {.input = {.name = "outlasts a press-through, and she stands on the leg (lab-B, 2026-11-12 04:58)", .presses = 3, .stands = true}, .expectation = {.arrives = true}},
    {.input = {.name = "ends in the fight, and she stands on the leg (lab-B, 2026-11-12 06:12)", .presses = 1, .stands = true}, .expectation = {.arrives = true}},
};

//  The plaza's greeter on an open leg, as lab-B saw her (2026-11-12 07:44,
//  00.05c below_the_company_clown, run 8): her talk stopped "south 5 (open)"
//  from (46, 27) with no tap counted, so the stop could be any tile of the
//  leg, (46, 27) to (46, 33); the walk planned from all seven, and her next
//  talk, a tap later, spread it over two columns, from which no plan was
//  found. Here her talk starts on each of `talks` tiles in turn (map tiles),
//  and the console's count of taps before a talk is lost (UncountedTalkIO).
struct OpenLegTalkInput{
    std::string name;
    std::vector<TilePos> talks;
};
struct OpenLegTalkExpectation{
    bool arrives;
};
struct OpenLegTalkCase{
    OpenLegTalkInput input;
    OpenLegTalkExpectation expectation;
};

//  Taps a talk stopped count none moved, as on lab-B (run 8).
class UncountedTalkIO : public RouteIO{
public:
    explicit UncountedTalkIO(RouteIO& io)
        : m_io(io)
    {}
    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override{
        return m_io.walk_to_wall(walk);
    }
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string& label) override{
        WalkResult result = m_io.walk_taps(direction, tiles, label);
        if (result.stop == WalkStop::DIALOG){
            result.moved_chunks = 0;
        }
        return result;
    }
    virtual void press(const RouteInput& input) override{
        m_io.press(input);
    }
    virtual RouteSight sight_within(const RouteWatch& watch) override{
        return m_io.sight_within(watch);
    }
    virtual bool holding_against_wall(Direction direction, const std::string& label) override{
        return m_io.holding_against_wall(direction, label);
    }
    virtual void look(const std::function<void(const ImageViewRGB32& screen)>& at) override{
        m_io.look(at);
    }
    virtual void note(const std::string& text, RouteNote kind) override{
        m_io.note(text, kind);
    }

private:
    RouteIO& m_io;
};

//  Where the walk from (46, 27) past the greeter's talks went wrong, or "".
//  It walks to the Poketch Company's clown, as on lab-B, on the plaza
//  ground of 00.05c's walk to the president (the old walk's, unchanged).
std::string walk_open_leg_talks(Logger& logger, const OpenLegTalkInput& input){
    const StoryStep* step = nullptr;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& s : row.steps){
            step = row.id == "00.05c" && s.name == "beside_the_president" ? &s : step;
        }
    }
    const StoryWorld* world = story_plan_world("JubilifeCity", "JubilifeCity");
    if (step == nullptr || world == nullptr){
        return "00.05c beside_the_president: not in the table, or no JubilifeCity world.";
    }
    const TilePos start = story_to_world(*world, "JubilifeCity", {46, 27});
    const TilePos target = story_to_world(*world, "JubilifeCity", {15, 19});
    GridSim sim({.world = world, .trainers = {}, .triggers = {}}, start, Direction::SOUTH, true);
    size_t talks = 0;
    const auto arm = [&](){
        if (talks < input.talks.size()){
            sim_add_trigger(sim, {.name = std::format("the greeter {}", talks),
                .tiles = {story_to_world(*world, "JubilifeCity", input.talks[talks])}, .spends = {}});
        }
    };
    arm();
    std::vector<std::string> notes;
    GridSimRouteIO sim_io(sim, notes);
    UncountedTalkIO io(sim_io);
    RouteTrainers trainers{{}, [&](const std::string&){
        sim.end_dialogue();
        talks++;
        arm();
        return false;
    }};
    std::vector<TilePos> believed;
    std::string error;
    try{
        believed = walk_route(io, *world, {start}, trainers, {.goal = [&](const TilePos& pos){ return pos == target; },
            .label = "beside the president", .avoid = story_route_tiles(*world, *step, "avoid"), .costs = *story_route_costs("town"),
            .speed = MoveSpeed::RUN, .scene_next = false, .goal_is_trigger = false, .greeter = story_route_tiles(*world, *step, "greeter")});
    }catch (const GridSimFailure& e){
        error = std::format("failed after {} talk(s): {}", talks, e.message);
    }
    for (const std::string& note : notes){
        logger.log(note);
    }
    if (!error.empty()){
        return error;
    }
    const TilePos at = nearest_tile(sim.player().pos);
    const bool arrived = believed == std::vector<TilePos>{target} && at == target && talks == input.talks.size();
    return arrived ? "" : std::format("ended on {} after {} talk(s), not on {}.", describe(*world, at), talks, describe(*world, target));
}

class Test_OpenLegTalkSim : public UnitTest{
public:
    explicit Test_OpenLegTalkSim(OpenLegTalkCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: 00.05c, the greeter's talk on an open leg " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string error = walk_open_leg_talks(logger, m_case.input);
        if (error.empty() != m_case.expectation.arrives){
            return error.empty() ? "arrived" : error;
        }
        return true;
    }

private:
    OpenLegTalkCase m_case;
};

const OpenLegTalkCase OPEN_LEG_TALK_SIM_CASES[] = {
    {.input = {.name = "with no tap counted", .talks = {{46, 29}}}, .expectation = {.arrives = true}},
    {.input = {.name = "with no tap counted, then again a tap on (lab-B, 2026-11-12 07:44)", .talks = {{46, 29}, {45, 30}}}, .expectation = {.arrives = true}},
};

//  lab-B 2026-11-12 21:01, 01.02 past_curtis: camper_1's sight on column 17
//  stopped the taps on (17, 22), and the count read "blocked after 9 of 13"
//  with the dialogue on the next tap: the walk placed us a tile or two back,
//  marked the beaten camper's tile, and planned a loop round it. In the Gate
//  the only way on is the east door along row 22, so any fight on the way
//  ends in a hold east to the door's fade, fighting what stops us on the way
//  (picnicker_0 on column 24).
class DialogOnNextTapIO : public GridSimRouteIO{
public:
    using GridSimRouteIO::GridSimRouteIO;
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string& label) override{
        if (m_dialog_next){
            m_dialog_next = false;
            return {.stop = WalkStop::DIALOG, .walked = 800ms, .wild_battles = 0, .moved_chunks = 0};
        }
        WalkResult result = GridSimRouteIO::walk_taps(direction, tiles, label);
        if (result.stop == WalkStop::DIALOG){
            result.stop = WalkStop::BLOCKED;
            result.moved_chunks = result.moved_chunks > 0 ? result.moved_chunks - 1 : 0;
            m_dialog_next = true;
        }
        return result;
    }

private:
    bool m_dialog_next = false;
};

std::string leave_the_gate_by_the_east_door(Logger& logger, bool leave){
    const StoryWorld* world = story_plan_world("OreburghGate", "OreburghGate");
    if (world == nullptr){
        return "no OreburghGate world.";
    }
    //  The Gate's trainers, as the game has them; the walk's map lists none
    //  of them (map_trainers() has no OreburghGate), so they are people it lacks.
    const std::vector<TrainerSight> gate_trainers{
        {.name = "camper_1", .pos = story_to_world(*world, "OreburghGate", {17, 25}), .facing = Direction::NORTH, .range = 4},
        {.name = "picnicker_0", .pos = story_to_world(*world, "OreburghGate", {24, 21}), .facing = Direction::SOUTH, .range = 2},
    };
    const TilePos start = story_to_world(*world, "OreburghGate", {7, 22});
    const TilePos target = story_to_world(*world, "OreburghGate", {20, 22});
    GridSim sim({.world = world, .trainers = gate_trainers, .triggers = {}}, start, Direction::EAST, true);
    std::vector<std::string> notes;
    DialogOnNextTapIO io(sim, notes);
    std::vector<std::string> fought;
    RouteTrainers trainers{{}, [&](const std::string& name){
        fought.push_back(name);
        sim.end_dialogue();
        return true;
    }};
    RouteEnd end;
    std::string error;
    try{
        end = walk_route_to_warp(io, *world, {start}, trainers, {.goal = [&](const TilePos& pos){ return pos == target; },
            .label = "past curtis", .costs = *story_route_costs("town"), .speed = MoveSpeed::RUN,
            .leave_toward = leave ? std::optional<Direction>(Direction::EAST) : std::nullopt});
    }catch (const GridSimFailure& e){
        error = e.message;
    }
    for (const std::string& note : notes){
        logger.log(note);
    }
    if (!error.empty()){
        return error;
    }
    if (!end.warp.any() || !sim.player().warped){
        return std::format("did not leave by the east door: ended on {} after {} fight(s).",
            describe(*world, nearest_tile(sim.player().pos)), fought.size());
    }
    return fought.size() >= 2 ? "" : std::format("left after {} fight(s), not camper_1 and picnicker_0.", fought.size());
}

struct GateEastOutInput{
    std::string name;
};
struct GateEastOutExpectation{
    bool leaves;
};
struct GateEastOutCase{
    GateEastOutInput input;
    GateEastOutExpectation expectation;
};

class Test_GateEastOut : public UnitTest{
public:
    explicit Test_GateEastOut(GateEastOutCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: 01.02 past_curtis, " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string error = leave_the_gate_by_the_east_door(logger, true);
        if (error.empty() != m_case.expectation.leaves){
            return error.empty() ? "left" : error;
        }
        return true;
    }

private:
    GateEastOutCase m_case;
};

const GateEastOutCase GATE_EAST_OUT_CASES[] = {
    {.input = {.name = "a fight in the Gate ends in a hold east through the door (lab-B 2026-11-12 21:01)"}, .expectation = {.leaves = true}},
};

} // namespace


void add_tests_AutoStory_WallBattleSim(UnitTestDatabase& database){
    for (const GateEastOutCase& item : GATE_EAST_OUT_CASES){
        database.add<Test_GateEastOut>(item);
    }
    for (const OpenLegTalkCase& item : OPEN_LEG_TALK_SIM_CASES){
        database.add<Test_OpenLegTalkSim>(item);
    }
    for (const TalkerCase& item : TALKER_SIM_CASES){
        database.add<Test_TalkerSim>(item);
    }
    for (const GreeterCase& item : GREETER_SIM_CASES){
        database.add<Test_GreeterSim>(item);
    }
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
