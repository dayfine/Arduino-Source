/*  BDSP AutoStory Planned Route Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  walk_route() (Utils/PokemonBDSP_AutoStory_PlannedRoute.h) on a script
 *  of console outcomes: each case lists the walks, presses and screen reads
 *  the walker must make, in order, and what the console answers. A call the
 *  script does not expect fails the case.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Data/PokemonBDSP_AutoStory_StoryTrainers.h"
#include "Data/PokemonBDSP_AutoStory_TrainerSight.h"
#include "Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "Utils/PokemonBDSP_AutoStory_StoryLanguage.h"
#include "Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "Utils/PokemonBDSP_AutoStory_RouteIO.h"
#include "Utils/PokemonBDSP_AutoStory_RouteTiming.h"
#include "PokemonBDSP_AutoStory_PlannedRoute_Tests.h"
#include "Utils/PokemonBDSP_AutoStory_WalkResult.h"

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


//  One call the walker is expected to make, and what the console answers:
//  `walk` for a walk, `answer` for a dialogue watch or the holding check.
struct ScriptStep{
    std::string call;
    WalkResult walk;
    bool answer;
};

ScriptStep walk_step(std::string call, WalkStop stop, Milliseconds walked, size_t moved){
    return {.call = std::move(call), .walk = {.stop = stop, .walked = walked, .wild_battles = 0, .moved_chunks = moved}, .answer = false};
}
ScriptStep answer_step(std::string call, bool answer){
    return {.call = std::move(call), .walk = {.stop = WalkStop::LIMIT, .walked = 0ms, .wild_battles = 0, .moved_chunks = 0}, .answer = answer};
}
ScriptStep press_step(std::string call){
    return answer_step(std::move(call), false);
}

//  What the console did on a scripted walk: the steps called so far, every
//  line logged, and the legs walked (to a wall, or a number of taps).
struct Script{
    std::vector<ScriptStep> steps;
    size_t next = 0;
    std::vector<std::string> notes = {};
    std::vector<TileLeg> legs = {};
};

//  Thrown by ScriptedRouteIO: a call the script did not expect, or the
//  walker failing the program.
struct ScriptMismatch{
    std::string message;
};
struct RouteFailed{
    std::string message;
};

std::string describe(const RouteInput& input){
    const std::string_view kind =
        input.kind == RouteInputKind::DPAD ? "D-pad" :
        input.kind == RouteInputKind::STICK ? "stick" :
        input.kind == RouteInputKind::BUTTON_A ? "A" :
        input.kind == RouteInputKind::WAIT ? "wait" : "pause";
    const bool directed = input.kind == RouteInputKind::DPAD || input.kind == RouteInputKind::STICK;
    return std::format("{}{}{} {} ms x{}", kind, directed ? " " : "", directed ? Pokemon::direction_name(input.direction) : "",
        input.hold.count(), input.count);
}

//  The console as a script: each call must be the script's next step.
//  look() sees no screen.
class ScriptedRouteIO : public RouteIO{
public:
    explicit ScriptedRouteIO(Script& script)
        : m_script(script)
    {}

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override{
        m_script.legs.push_back({.direction = walk.direction, .tiles = 0, .ends_at_wall = true});
        return expect("wall: " + walk.label).walk;
    }
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string& label) override{
        m_script.legs.push_back({.direction = direction, .tiles = tiles, .ends_at_wall = false});
        return expect(std::format("{} taps: {}", tiles, label)).walk;
    }
    virtual void press(const RouteInput& input) override{
        expect(describe(input));
    }
    virtual bool dialog_within(Milliseconds wait, Color) override{
        return expect(std::format("dialog within {} ms", wait.count())).answer;
    }
    virtual bool holding_against_wall(Direction direction, const std::string&) override{
        return expect(std::string("holding ") + Pokemon::direction_name(direction)).answer;
    }
    virtual void look(const std::function<void(const ImageViewRGB32& screen)>&) override{}
    virtual void note(const std::string& text, RouteNote kind) override{
        m_script.notes.push_back(text);
        if (kind == RouteNote::FAILURE){
            throw RouteFailed{text};
        }
    }

private:
    const ScriptStep& expect(const std::string& call){
        if (m_script.next >= m_script.steps.size()){
            throw ScriptMismatch{"a call after the script's end: \"" + call + "\""};
        }
        const ScriptStep& step = m_script.steps[m_script.next++];
        if (step.call != call){
            throw ScriptMismatch{std::format("step {}: expected \"{}\", got \"{}\"", m_script.next, step.call, call)};
        }
        return step;
    }

    Script& m_script;
};


//  How a scripted walk ended: where it arrived, or why it failed.
struct ScriptedWalk{
    std::vector<TilePos> at;
    std::string failure;
    std::vector<std::string> fights;
};

ScriptedWalk walk_script(const StoryWorld& maps, const TilePos& from, const TilePos& goal, std::vector<TrainerSight> sights, Script& script){
    ScriptedWalk out;
    RouteTrainers trainers{
        .sights = std::move(sights),
        .fight = [&out](const std::string& trainer){ out.fights.push_back(trainer); },
    };
    ScriptedRouteIO io(script);
    try{
        out.at = walk_route(io, maps, {from}, trainers, {
            .goal = [goal](const TilePos& pos){ return pos == goal; },
            .label = "T",
            .avoid = {},
            .costs = route_plan_costs(),
            .speed = MoveSpeed::WALK,
        });
    }catch (const RouteFailed& e){
        out.failure = e.message;
    }
    return out;
}

//  The script's calls all made, or which are left.
std::string script_left(const Script& script){
    if (script.next == script.steps.size()){
        return "";
    }
    return std::format("{} of {} steps called; next \"{}\".", script.next, script.steps.size(), script.steps[script.next].call);
}

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


//  A leg that starts is logged as the route log writes it, then paused
//  before, in LEGS mode: its place, direction, tiles and the one tile it
//  ends on; no destination when it may end on several.
UnitTestResult test_pause_before_leg(Logger&){
    Script script;
    ScriptedRouteIO io(script);
    std::vector<std::string> paused;
    Pokemon::StoryPausePoints pause(Pokemon::StoryBreakpoints::LEGS, [&paused](std::string_view label){
        paused.emplace_back(label);
    });
    pause.before_step("01.04/3 gym");
    const Pokemon::PlannedRouteLeg one{
        .leg = {.direction = Direction::EAST, .tiles = 4}, .after = {{.map = "Test", .pos = {10, 10}}}
    };
    Pokemon::PlannedRouteLeg several = one;
    several.after.push_back({.map = "Test", .pos = {11, 10}});
    io.leg_started(2, 5, one, &pause);
    io.leg_started(3, 5, several, &pause);
    io.leg_started(4, 5, one, nullptr);
    const std::vector<std::string> expected{
        "01.04/3 gym", "01.04/3 gym leg 2/5 east 4 -> (10, 10)", "01.04/3 gym leg 3/5 east 4",
    };
    if (paused != expected || script.notes.size() != 3){
        return std::format("Expected {} pauses and 3 log lines, got {} and {}.", expected.size(), paused.size(), script.notes.size());
    }
    return true;
}

//  r54: a trainer's line froze the check walk at a leg's wall; after the
//  battle the walk on reached the wall, but the check still judged the
//  frozen walk's DIALOG and failed. The walk on is what counts.
UnitTestResult test_wall_leg_trainer_on_the_way(Logger&){
        Script script{.steps = {
            walk_step("wall: T: east 4 (to wall)", WalkStop::LIMIT, 2050ms, 0),
            walk_step("wall: T: east 4 (to wall): at the wall?", WalkStop::DIALOG, 400ms, 0),
            walk_step("wall: T: east 4 (to wall): on to the wall", WalkStop::BLOCKED, 1200ms, 0),
            answer_step("dialog within 500 ms", false),
        }};
        try{
            const ScriptedWalk walk = walk_script(make_world({"#.....#"}), {1, 0}, {5, 0}, {}, script);
            if (!walk.failure.empty()){
                return "failed after the walk on reached the wall: " + walk.failure;
            }
            if (walk.fights != std::vector<std::string>{"a trainer on the way"}){
                return std::format("fought {} times, expected once.", walk.fights.size());
            }
            if (walk.at != std::vector<TilePos>{{5, 0}}){
                return "did not arrive at (5, 0).";
            }
        }catch (const ScriptMismatch& e){
            return e.message;
        }
        const std::string left = script_left(script);
        return left.empty() ? UnitTestResult(true) : UnitTestResult(left);
}


//  A leg to a wall that ran out of time: a check walk that also runs out
//  of time counts as at the wall only if holding on there changes the
//  screen less than walking (r127a ran along the wrong row).
struct WallCheckCase{
    std::string name;
    std::vector<ScriptStep> check;
    bool fails;
};

UnitTestResult test_wall_leg_limit(Logger&){
        const std::vector<WallCheckCase> cases{
            {.name = "the check stops at the wall", .check = {
                walk_step("wall: T: east 4 (to wall): at the wall?", WalkStop::BLOCKED, 1050ms, 0),
                answer_step("dialog within 500 ms", false),
            }, .fails = false},
            {.name = "the check runs out of time, holding on reads a wall", .check = {
                walk_step("wall: T: east 4 (to wall): at the wall?", WalkStop::LIMIT, 2000ms, 0),
                answer_step("holding east", true),
                answer_step("dialog within 500 ms", false),
            }, .fails = false},
            {.name = "the check runs out of time, holding on still moves", .check = {
                walk_step("wall: T: east 4 (to wall): at the wall?", WalkStop::LIMIT, 2000ms, 0),
                answer_step("holding east", false),
            }, .fails = true},
        };
        for (const WallCheckCase& item : cases){
            Script script{.steps = {walk_step("wall: T: east 4 (to wall)", WalkStop::LIMIT, 2050ms, 0)}};
            script.steps.insert(script.steps.end(), item.check.begin(), item.check.end());
            try{
                const ScriptedWalk walk = walk_script(make_world({"#.....#"}), {1, 0}, {5, 0}, {}, script);
                if (walk.failure.empty() == item.fails){
                    return item.name + (item.fails ? ": went on as at the wall." : ": failed: " + walk.failure);
                }
            }catch (const ScriptMismatch& e){
                return item.name + ": " + e.message;
            }
            const std::string left = script_left(script);
            if (!left.empty()){
                return item.name + ": " + left;
            }
        }
        return true;
}


//  Whether walking `legs` from `from` ever steps into `person`'s tile: a
//  leg to a wall that stops against them, or a tap toward them.
bool walks_into(const StoryWorld& world, TilePos from, const std::vector<TileLeg>& legs, const TilePos& person){
    for (const TileLeg& leg : legs){
        for (size_t tap = 0; leg.ends_at_wall || tap < leg.tiles; tap++){
            if (from.next(leg.direction) == person){
                return true;
            }
            const std::optional<TilePos> next = step(world, from, leg.direction);
            if (!next){
                break;
            }
            from = *next;
        }
    }
    return false;
}

//  Route 202 alone, at its own tiles, and Logan's sight: he stands at
//  (26, 4) and looks south.
StoryWorld route202_world(){
    StoryWorld world;
    world.add("Route 202", story_map("Route202"), {0, 0});
    return world;
}
std::vector<TrainerSight> logan_sight(){
    std::vector<TrainerSight> sights;
    for (const TrainerSight& sight : route202_trainer_sights()){
        if (sight.name == "logan"){
            sights.push_back(sight);
        }
    }
    return sights;
}

//  Whether the replan after the battle starts from each of `tiles`.
std::string check_replan_from(const Script& script, const std::vector<std::string>& tiles){
    const auto replan = std::find_if(script.notes.begin(), script.notes.end(), [](const std::string& note){
        return note.find("planning again") != std::string::npos;
    });
    if (replan == script.notes.end()){
        return "never planned again after the battle.";
    }
    for (const std::string& tile : tiles){
        if (replan->find(tile) == std::string::npos){
            return "the replan does not start from " + tile + ": " + *replan;
        }
    }
    return "";
}

//  r55: Logan (Route 202, (26, 4), looking south) stopped an open leg west
//  along row 5. "5 taps" were 4, under his nose at (26, 5); the replan from
//  the tap count alone walked into him. The replan must start from both the
//  tile the taps give and the tile where the leg enters his sight, and walk
//  on from either without bumping into him.
UnitTestResult test_open_leg_known_trainer(Logger&){
        const StoryWorld world = route202_world();
        //  West along row 5 from (30, 5) to (24, 5): his line is the 4th tap,
        //  and his dialogue comes after 3 taps that moved.
        Script script{.steps = {
            press_step("D-pad west 50 ms x1"),
            walk_step("6 taps: T: west 6 (open)", WalkStop::DIALOG, 1500ms, 3),
            walk_step("wall: T: east 4 (to wall)", WalkStop::BLOCKED, 2000ms, 0),
            answer_step("dialog within 500 ms", false),
            press_step("D-pad west 50 ms x1"),
            walk_step("6 taps: T: west 6 (open)", WalkStop::LIMIT, 1500ms, 6),
            answer_step("dialog within 500 ms", false),
        }};
        ScriptedWalk walk;
        try{
            walk = walk_script(world, {30, 5}, {24, 5}, logan_sight(), script);
        }catch (const ScriptMismatch& e){
            return e.message;
        }
        if (!walk.failure.empty() || walk.fights != std::vector<std::string>{"logan"}){
            return std::format("expected to fight Logan once and go on; fought {}, failure \"{}\".", walk.fights.size(), walk.failure);
        }
        //  The taps say (27, 5); his sight line crosses the row at (26, 5).
        const std::string replan = check_replan_from(script, {"Route 202 (27, 5)", "Route 202 (26, 5)"});
        if (!replan.empty()){
            return replan;
        }
        const std::vector<TileLeg> after_battle(script.legs.begin() + 1, script.legs.end());
        const TilePos logan = map_point("Route202", "trainer_logan");
        for (const TilePos& from : {TilePos{27, 5}, TilePos{26, 5}}){
            if (walks_into(world, from, after_battle, logan)){
                return "the replan walks into Logan from " + Pokemon::to_string(from) + ".";
            }
        }
        if (walk.at != std::vector<TilePos>{{24, 5}}){
            return "did not arrive at (24, 5).";
        }
        const std::string left = script_left(script);
        return left.empty() ? UnitTestResult(true) : UnitTestResult(left);
}


//  The console as a small simulator: the player stands on one tile of
//  `world`, walls and `obstacles` stop them, and every walk and tap moves
//  them. Fails the case after `max_calls` calls (a walker that loops).
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
        const Milliseconds per_tile = route_ms_per_tile(walk.direction, MoveSpeed::WALK);
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
UnitTestResult test_jubilife_center_held_overshoot(Logger&){
    const StoryWorld world = jubilife_world();
    const TilePos from{48, 62};
    const TilePos goal{52, 41};
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


struct PlannedRouteInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct PlannedRouteExpectation{
};

struct PlannedRouteCase{
    PlannedRouteInput input;
    PlannedRouteExpectation expectation;
};

class Test_PlannedRoute : public UnitTest{
public:
    explicit Test_PlannedRoute(PlannedRouteCase test_case)
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
    PlannedRouteCase m_case;
};

} // namespace


const PlannedRouteCase PLANNED_ROUTE_CASES[] = {
    {
        .input = {"a leg's breakpoint", test_pause_before_leg},
        .expectation = {},
    },
    {
        .input = {"a trainer on a wall leg, then on to the wall", test_wall_leg_trainer_on_the_way},
        .expectation = {},
    },
    {
        .input = {"a wall leg out of time needs the holding check", test_wall_leg_limit},
        .expectation = {},
    },
    {
        .input = {"a known trainer stops an open leg", test_open_leg_known_trainer},
        .expectation = {},
    },
    {
        .input = {"blocked from several possible tiles routes around", test_blocked_from_several_tiles},
        .expectation = {},
    },
    {
        .input = {"Jubilife to the Center with timed holds overshooting", test_jubilife_center_held_overshoot},
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

void add_tests_AutoStory_PlannedRoute(UnitTestDatabase& database){
    for (const auto& item : PLANNED_ROUTE_CASES){
        database.add<Test_PlannedRoute>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
