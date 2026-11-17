/*  BDSP AutoStory Simulated Route Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  walk_route() (Utils/PokemonBDSP_AutoStory_PlannedRoute.h) on a small
 *  simulated console (SimulatedRouteIO): walls and obstacles stop the
 *  player, and timed holds can overshoot. Covers the Jubilife Center walks
 *  from lab-B and a fault sweep over every ROUTE step of the story table.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Data/PokemonBDSP_AutoStory_StoryTrainers.h"
#include "Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "Utils/PokemonBDSP_AutoStory_RouteIO.h"
#include "Utils/PokemonBDSP_AutoStory_RouteTiming.h"
#include "Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_SimulatedRoute_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::Pose;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;


//  Thrown by SimulatedRouteIO when the walker fails the program.
struct RouteFailed{
    std::string message;
};


//  The console as a small simulator: the player stands on one tile of
//  `world`, walls and `obstacles` stop them, and every walk and tap moves
//  them. Fails the case after `max_calls` calls (a walker that loops).
// code-health: allow public_methods -- a RouteIO fake: the interface's seven overrides plus three test knobs
class SimulatedRouteIO : public RouteIO{
public:
    SimulatedRouteIO(const StoryWorld& world, TilePos at, std::vector<TilePos> obstacles, std::vector<std::string>& notes)
        : m_world(world)
        , m_at(at)
        , m_obstacles(std::move(obstacles))
        , m_notes(notes)
    {}

    const TilePos& at() const{
        return m_at;
    }
    size_t tiles_walked() const{
        return m_walked;
    }

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override{
        count_call();
        const MoveSpeed speed = walk.options.pace == DpadPace::RUN ? MoveSpeed::RUN : MoveSpeed::WALK;
        const Milliseconds per_tile = route_ms_per_tile(walk.direction, speed);
        if (m_overshoot && walk.limit > ROUTE_START_DELAY){
            //  A hold timed for n tiles (ROUTE_START_DELAY, then half a
            //  tile under n) goes on past them until a wall stops it.
            const size_t tiles = (size_t)((walk.limit - ROUTE_START_DELAY + per_tile / 2) / per_tile);
            const size_t want = tiles + (tiles * 35 + 99) / 100;
            const size_t moved = move(walk.direction, want);
            return {.stop = moved < want ? WalkStop::BLOCKED : WalkStop::LIMIT, .walked = walk.limit,
                .wild_battles = 0, .moved_chunks = moved};
        }
        const size_t moved = move(walk.direction, SIZE_MAX);
        return {.stop = WalkStop::BLOCKED, .walked = ROUTE_START_DELAY + 800ms + per_tile * moved + per_tile / 2,
            .wild_battles = 0, .moved_chunks = moved};
    }
    //  Fault class "a timed hold overshoots": a stick hold timed for n
    //  tiles goes 0..ceil(0.35 n) tiles further, unseen, until a wall stops
    //  it (lab-B 2026-11-11, Jubilife: runs timed for 10 tiles went about 13,
    //  three of three). D-pad taps are tile-exact (5 to 20 taps, every one
    //  moved) and walks to a wall end at the wall, so neither has a fault.
    void overshoot_timed_holds(bool overshoot){
        m_overshoot = overshoot;
    }
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string&) override{
        count_call();
        const size_t moved = move(direction, tiles);
        return {.stop = moved < tiles ? WalkStop::BLOCKED : WalkStop::LIMIT, .walked = 250ms * tiles,
            .wild_battles = 0, .moved_chunks = moved};
    }
    virtual void press(const RouteInput& input) override{
        //  arrive() pauses for up to 3 s for a location banner the sim never
        //  shows: those pauses are not a walker that loops.
        if (input.kind != RouteInputKind::PAUSE){
            count_call();
        }
        //  A tap under 100 ms only turns (route_turn_tap()).
        if (input.kind == RouteInputKind::DPAD && input.hold >= 100ms){
            move(input.direction, input.count);
        }
        //  A stick hold is timed: route_open_hold() is half a tile under n.
        if (input.kind == RouteInputKind::STICK){
            const Milliseconds per_tile = route_ms_per_tile(input.direction, MoveSpeed::WALK);
            const size_t tiles = (size_t)((input.hold + per_tile / 2) / per_tile);
            const size_t over = m_overshoot ? (tiles * 35 + 99) / 100 : 0;
            for (size_t press = 0; press < input.count; press++){
                move(input.direction, tiles + over);
            }
        }
    }
    virtual bool dialog_within(Milliseconds, Color) override{
        count_call();
        return false;
    }
    virtual bool holding_against_wall(Direction, const std::string&) override{
        count_call();
        return true;
    }
    virtual void look(const std::function<void(const ImageViewRGB32& screen)>&) override{}
    virtual void note(const std::string& text, RouteNote kind) override{
        m_notes.push_back(text);
        if (kind == RouteNote::FAILURE){
            throw RouteFailed{text};
        }
    }

private:
    void count_call(){
        if (++m_calls > MAX_CALLS){
            throw RouteFailed{"the walker made more than " + std::to_string(MAX_CALLS) + " calls."};
        }
    }
    size_t move(Direction direction, size_t tiles){
        size_t moved = 0;
        while (moved < tiles){
            const std::optional<TilePos> next = step(m_world, m_at, direction);
            if (!next || std::find(m_obstacles.begin(), m_obstacles.end(), *next) != m_obstacles.end()){
                break;
            }
            m_at = *next;
            moved++;
        }
        m_walked += moved;
        return moved;
    }

    static constexpr size_t MAX_CALLS = 300;
    const StoryWorld& m_world;
    TilePos m_at;
    std::vector<TilePos> m_obstacles;
    std::vector<std::string>& m_notes;
    size_t m_calls = 0;
    size_t m_walked = 0;
    bool m_overshoot = false;
};

StoryWorld jubilife_world(){
    StoryWorld world;
    world.add("Jubilife City", story_map("JubilifeCity"), {0, 0});
    return world;
}

//  lab-B 2026-11-11 11:17 (00.05 to_the_center): a walk east stopped about
//  13 tiles short of the wall, which left 15 possible tiles, (49..53,
//  41..43). Something the map lacks stood east of us; with several possible
//  tiles nothing was marked, so every replan chose "east 7 (to wall)" again,
//  walked 0 tiles five times and gave up on "too many plans". A walk that
//  never moved from several possible tiles must route around the tiles just
//  past them.
UnitTestResult test_blocked_from_several_tiles(Logger&){
    const StoryWorld world = jubilife_world();
    std::vector<TilePos> from;
    for (int y = 41; y <= 43; y++){
        for (int x = 49; x <= 53; x++){
            from.push_back({x, y});
        }
    }
    const TilePos goal{52, 41};
    std::vector<std::string> notes;
    SimulatedRouteIO io(world, {53, 41}, {{54, 41}}, notes);
    RouteTrainers trainers{.sights = {}, .fight = [](const std::string&){}};
    try{
        walk_route(io, world, from, trainers, {
            .goal = [goal](const TilePos& pos){ return pos == goal; },
            .label = "T",
            .avoid = {},
            .costs = town_plan_costs(),
            .speed = MoveSpeed::WALK,
        });
    }catch (const RouteFailed& e){
        return "failed: " + e.message;
    }
    if (io.at() != goal){
        return "ended at " + Pokemon::to_string(io.at()) + ", not at (52, 41).";
    }
    return true;
}

size_t tiles_of(const std::vector<PlannedLeg>& legs){
    size_t tiles = 0;
    for (const PlannedLeg& leg : legs){
        tiles += leg.leg.tiles;
    }
    return tiles;
}

//  The fewest tiles from `from` to `goal` on Jubilife, open legs exact.
size_t jubilife_shortest(const StoryWorld& world, TilePos from, TilePos goal){
    const auto plan = plan_robust_route(world, from, goal, waypoint_plan_costs());
    return plan ? tiles_of(*plan) : 0;
}

//  lab-B 2026-11-11 13:38 (00.05 to_the_center from (48, 62)): "north 20
//  (open)" walked as a held run (3175 ms) overshot, the west leg after it
//  stopped 9 tiles in, the walker put walls at (54, 41..43) and "arrived"
//  at (52, 41) nowhere near the Center. Timed holds overshoot by up to
//  ceil(0.35 n) tiles (three runs of 10 went about 13); taps are exact.
//  With every timed hold overshooting the walk still ends on the tile.
//  Run (the program after the shoes, 00.05 heal_jubilife) and walk alike.
UnitTestResult test_jubilife_center_held_overshoot(Logger&){
    const StoryWorld world = jubilife_world();
    const TilePos from{48, 62};
    const TilePos goal{52, 41};
    for (const MoveSpeed speed : {MoveSpeed::RUN, MoveSpeed::WALK}){
        const std::string_view pace = speed == MoveSpeed::RUN ? "running" : "walking";
        std::vector<std::string> notes;
        SimulatedRouteIO io(world, from, {}, notes);
        io.overshoot_timed_holds(true);
        RouteTrainers trainers{.sights = {}, .fight = [](const std::string&){}};
        try{
            walk_route(io, world, {from}, trainers, {
                .goal = [goal](const TilePos& pos){ return pos == goal; },
                .label = "T",
                .avoid = {},
                .costs = town_plan_costs(),
                .speed = speed,
            });
        }catch (const RouteFailed& e){
            return std::format("{}: failed: {}", pace, e.message);
        }
        if (io.at() != goal){
            return std::format("{}: ended at {}, not at (52, 41).", pace, Pokemon::to_string(io.at()));
        }
    }
    return true;
}

//  The same walk on the console: the kids at (52, 46) and (54, 46) stop a
//  walk north up their columns. The walker replans from where it stopped
//  and reaches the Center without a long way round.
UnitTestResult test_jubilife_center_kids(Logger&){
    const StoryWorld world = jubilife_world();
    const TilePos from{48, 62};
    const TilePos goal{52, 41};
    std::vector<std::string> notes;
    SimulatedRouteIO io(world, from, {{52, 46}, {54, 46}}, notes);
    RouteTrainers trainers{.sights = {}, .fight = [](const std::string&){}};
    try{
        walk_route(io, world, {from}, trainers, {
            .goal = [goal](const TilePos& pos){ return pos == goal; },
            .label = "T",
            .avoid = {},
            .costs = town_plan_costs(),
            .speed = MoveSpeed::WALK,
        });
    }catch (const RouteFailed& e){
        return "failed: " + e.message;
    }
    if (io.at() != goal){
        return "ended at " + Pokemon::to_string(io.at()) + ", not at (52, 41).";
    }
    const size_t least = jubilife_shortest(world, from, goal);
    if (io.tiles_walked() * 10 > least * 13 + 40){
        return std::format("walked {} tiles where {} would do.", io.tiles_walked(), least);
    }
    return true;
}

//  One ROUTE step of the story table, from where the steps before it end.
struct StoryRoute{
    std::string name;
    Pose from;
    Pose goal;
    std::string costs;
};

//  Every ROUTE step whose start the table knows: a row's start, a ROUTE's
//  target or a step's pose_after. Steps after any other step start
//  unknown and are left out.
void add_story_route(const std::string& row, const StoryStep& step, std::optional<Pose>& at, std::vector<StoryRoute>& routes){
    if (step.kind == StoryStepKind::ROUTE){
        const Pose goal = Pokemon::pose_of(Pokemon::story_arg<GridPose>(step.args, "target"));
        if (at){
            routes.push_back({std::format("{} {}", row, step.name), *at, goal, step.action});
        }
        at = goal;
    }else{
        at.reset();
    }
    if (step.pose_after){
        at = Pokemon::pose_of(*step.pose_after);
    }
}
std::vector<StoryRoute> story_routes(){
    std::vector<StoryRoute> routes;
    for (const StoryCheckpoint& row : story_checkpoints()){
        std::optional<Pose> at;
        if (row.start){
            at = Pokemon::pose_of(*row.start);
        }
        for (const StoryStep& step : row.steps){
            add_story_route(row.id, step, at, routes);
        }
    }
    return routes;
}

//  Walks `route` from each of its start tiles on the simulated console,
//  every timed hold overshooting if `overshoot`. Returns the tiles walked
//  from each start, or why it failed.
std::string sweep_story_route(const StoryRoute& route, bool overshoot, std::vector<size_t>& walked){
    const StoryWorld* world = story_plan_world(route.from.map, route.goal.map);
    const std::optional<RobustPlanCosts> costs = story_route_costs(route.costs);
    if (world == nullptr || !costs){
        return route.name + ": no world or costs.";
    }
    std::vector<TilePos> targets;
    for (const TilePos& pos : Pokemon::tiles_of(route.goal)){
        targets.push_back(story_to_world(*world, route.goal.map, pos));
    }
    std::vector<TilePos> start;
    for (const TilePos& pos : Pokemon::tiles_of(route.from)){
        start.push_back(story_to_world(*world, route.from.map, pos));
    }
    for (const TilePos& from : start){
        std::vector<std::string> notes;
        SimulatedRouteIO io(*world, from, {}, notes);
        io.overshoot_timed_holds(overshoot);
        RouteTrainers trainers{.sights = story_trainers(*world), .fight = [](const std::string&){}};
        try{
            walk_route(io, *world, start, trainers, {
                .goal = [&](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); },
                .label = route.name,
                .avoid = {},
                .costs = *costs,
                .speed = MoveSpeed::WALK,
            });
        }catch (const RouteFailed& e){
            std::string last;
            for (size_t i = notes.size() > 4 ? notes.size() - 4 : 0; i < notes.size(); i++){
                last += "\n        " + notes[i];
            }
            return std::format("{} from {}{}: {}{}", route.name, Pokemon::to_string(from), overshoot ? ", holds overshooting" : "", e.message, last);
        }
        if (std::find(targets.begin(), targets.end(), io.at()) == targets.end()){
            return std::format("{} from {}{}: ended at {}.", route.name, Pokemon::to_string(from), overshoot ? ", holds overshooting" : "", Pokemon::to_string(io.at()));
        }
        walked.push_back(io.tiles_walked());
    }
    return "";
}

//  Fault sweep: every story route, walked with every timed hold
//  overshooting, still ends on its target. The log line per route gives the
//  most tiles walked from any of its start tiles, without the fault and with
//  it.
UnitTestResult test_story_routes_holds_overshoot(Logger& logger){
    std::string failures;
    const std::vector<StoryRoute> routes = story_routes();
    for (const StoryRoute& route : routes){
        std::string line = route.name + ":";
        for (bool overshoot : {false, true}){
            std::vector<size_t> walked;
            const std::string error = sweep_story_route(route, overshoot, walked);
            if (!error.empty()){
                failures += "\n    " + error;
                line += " FAIL";
                continue;
            }
            size_t most = 0;
            for (size_t tiles : walked){
                most = std::max(most, tiles);
            }
            line += " " + std::to_string(most);
        }
        logger.log("[timed holds overshoot: no, yes] " + line);
    }
    if (!failures.empty()){
        return std::format("{} routes swept:{}", routes.size(), failures);
    }
    return true;
}


struct SimulatedRouteInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct SimulatedRouteExpectation{
};

struct SimulatedRouteCase{
    SimulatedRouteInput input;
    SimulatedRouteExpectation expectation;
};

class Test_SimulatedRoute : public UnitTest{
public:
    explicit Test_SimulatedRoute(SimulatedRouteCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Planned route: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    SimulatedRouteCase m_case;
};

} // namespace


const SimulatedRouteCase SIMULATED_ROUTE_CASES[] = {
    {
        .input = {"blocked from several possible tiles routes around", test_blocked_from_several_tiles},
        .expectation = {},
    },
    {
        .input = {"Jubilife Center with every timed hold overshooting", test_jubilife_center_held_overshoot},
        .expectation = {},
    },
    {
        .input = {"Jubilife to the Center past the kids", test_jubilife_center_kids},
        .expectation = {},
    },
    {
        .input = {"fault sweep: every story route with timed holds overshooting", test_story_routes_holds_overshoot},
        .expectation = {},
    },
};

void add_tests_AutoStory_SimulatedRoute(UnitTestDatabase& database){
    for (const auto& item : SIMULATED_ROUTE_CASES){
        database.add<Test_SimulatedRoute>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
