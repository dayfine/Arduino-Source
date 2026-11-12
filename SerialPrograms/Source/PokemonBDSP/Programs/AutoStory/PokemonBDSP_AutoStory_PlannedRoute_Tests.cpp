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
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Data/PokemonBDSP_AutoStory_StoryTrainers.h"
#include "Data/PokemonBDSP_AutoStory_TrainerSight.h"
#include "Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "Utils/PokemonBDSP_AutoStory_RouteIO.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "Utils/PokemonBDSP_AutoStory_StoryLanguage.h"
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
        m_script.legs.push_back({.direction = walk.direction, .tiles = 0, .ends_at_wall = true, .held = false});
        return expect("wall: " + walk.label).walk;
    }
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string& label) override{
        m_script.legs.push_back({.direction = direction, .tiles = tiles, .ends_at_wall = false, .held = false});
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


//  01.08's walk below the Ravaged Path (lab-B 2026-11-08 16:50): the leg
//  north up Route 204 South ended at (13, 2), and the leg "west 2 (to
//  wall)" to (11, 2) stopped at once, 0 chunks moved: the wandering
//  youngster (the map's youngster_3, wandering (12, 2) to (14, 2)) stood on
//  (12, 2). The world joins Jubilife and Route 204 South, as the step's.
const StoryWorld& ravaged_path_world(){
    static const StoryWorld* const world = story_plan_world("JubilifeCity", "Route204South");
    return *world;
}
TilePos on_route204_south(TilePos local){
    return story_to_world(ravaged_path_world(), "Route204South", local);
}

//  Routes no longer run along a wander zone, so that leg is no longer
//  planned on Route 204 South: the walks below replay it on a plain room,
//  from (3, 1) "west 2 (to wall)" to (1, 1), the person on (2, 1), with a
//  row below to plan round them.
StoryWorld youngster_room(){
    return make_world({"#####", "#...#", "#...#", "#####"});
}

//  The console's first walk west: blocked after 800 ms, no chunk moved.
ScriptStep west_never_moved(){
    return walk_step("wall: T: west 2 (to wall)", WalkStop::BLOCKED, 1050ms, 0);
}

//  A youngster who steps off the leg's next tile while we wait is waited
//  out, on a leg of two tiles too: the walk west then reaches the wall at
//  (11, 2). The wait used to need a stop at least three tiles short of the
//  wall, so the person one tile ahead was talked to instead.
UnitTestResult test_short_leg_waits_for_the_way(Logger&){
    Script script{.steps = {
        west_never_moved(),
        press_step("wait 2500 ms x1"),
        walk_step("wall: T: west 2 (to wall)", WalkStop::BLOCKED, 3000ms, 2),
        answer_step("dialog within 500 ms", false),
    }};
    ScriptedWalk walk;
    try{
        walk = walk_script(youngster_room(), {3, 1}, {1, 1}, {}, script);
    }catch (const ScriptMismatch& e){
        return e.message;
    }
    if (!walk.failure.empty() || !walk.fights.empty()){
        return std::format("expected to wait and walk on; fought {}, failure \"{}\".", walk.fights.size(), walk.failure);
    }
    if (walk.at != std::vector<TilePos>{{1, 1}}){
        return "did not arrive at (1, 1).";
    }
    const std::string left = script_left(script);
    return left.empty() ? UnitTestResult(true) : UnitTestResult(left);
}

//  Whether the replan after the youngster's talk starts from (3, 1) alone,
//  with (2, 1) marked, and plans a route.
std::string check_never_moved_replan(const Script& script){
    const std::string replan = check_replan_from(script, {"(3, 1)"});
    if (!replan.empty()){
        return replan;
    }
    const auto replan_note = std::find_if(script.notes.begin(), script.notes.end(), [](const std::string& note){
        return note.find("planning again") != std::string::npos;
    });
    if (replan_note->find("(2, 1)") != std::string::npos){
        return "the replan still starts from the youngster's tile: " + *replan_note;
    }
    const auto marked = std::find_if(script.notes.begin(), script.notes.end(), [](const std::string& note){
        return note.find("marked it") != std::string::npos;
    });
    if (marked == script.notes.end() || marked->find("(2, 1)") == std::string::npos){
        return "the youngster's tile (2, 1) was not marked.";
    }
    const size_t plans = std::count_if(script.notes.begin(), script.notes.end(), [](const std::string& note){
        return note.find("legs planned") != std::string::npos;
    });
    if (plans != 2){
        return std::format("{} plans made, expected the first and the replan.", plans);
    }
    return "";
}

//  The youngster stays on (12, 2) through both waits and talks without a
//  battle. The console then planned from {(12, 2), (13, 2)}, one tile per
//  tile the walk may have moved, and found no plan: an uncertain start
//  needs a wall both tiles reach (wall_recovery_candidates()), and no wall
//  is anchored from a tile someone wanders on, which both are. A walk that
//  never moved stopped where it began: the replan starts from the start
//  alone, with the youngster's tile marked (on youngster_room()).
UnitTestResult test_never_moved_plans_from_the_start(Logger& logger){
    const StoryWorld& world = ravaged_path_world();
    const std::vector<TilePos> console_from{on_route204_south({12, 2}), on_route204_south({13, 2})};
    if (!wall_recovery_candidates(world, console_from).empty()){
        return "the console's two tiles have a wall to anchor on: the cause is not the wander box.";
    }
    logger.log("No wall anchors from Route 204 South (12, 2) and (13, 2): both are in youngster_3's wander box.");
    Script script{.steps = {
        west_never_moved(),
        press_step("wait 2500 ms x1"),
        west_never_moved(),
        press_step("wait 2500 ms x1"),
        west_never_moved(),
        press_step("A 80 ms x1"),
        answer_step("dialog within 2000 ms", true),
    }};
    ScriptedWalk walk;
    std::string stopped_at;
    try{
        walk = walk_script(youngster_room(), {3, 1}, {1, 1}, {}, script);
    }catch (const ScriptMismatch& e){
        //  The walk after the replan is not scripted.
        stopped_at = e.message;
    }
    if (!walk.failure.empty()){
        return "failed: " + walk.failure;
    }
    if (script.next != script.steps.size()){
        return "stopped at: " + stopped_at;
    }
    for (const std::string& note : script.notes){
        logger.log(note);
    }
    const std::string replan = check_never_moved_replan(script);
    return replan.empty() ? UnitTestResult(true) : UnitTestResult(replan);
}

//  A leg to the wall two tiles away that runs out of time after one tile
//  (one chunk moved): someone stands in it, as Route 204's youngster did on
//  01.08's "west 2 (to wall)" (lab-B 2026-11-08). The check at the wall
//  allowed a tile short on legs of two or three tiles, so the stop against
//  him passed as the wall and the route went on a tile off. Now it is a
//  stop short of the wall: talk to whoever is there, then plan again.
UnitTestResult test_short_leg_a_tile_short_is_not_arrived(Logger&){
    const StoryWorld room = make_world({"####", "#..#", "#..#", "#..#", "####"});
    Script script{.steps = {
        walk_step("wall: T: north 2 (to wall)", WalkStop::LIMIT, 1850ms, 1),
        press_step("A 80 ms x1"),
        answer_step("dialog within 2000 ms", false),
    }};
    ScriptedWalk walk;
    std::string stopped_at;
    try{
        walk = walk_script(room, {1, 3}, {1, 1}, {}, script);
    }catch (const ScriptMismatch& e){
        //  The walk after the replan is not scripted.
        stopped_at = e.message;
    }
    if (!walk.failure.empty()){
        return "failed: " + walk.failure;
    }
    if (script.next != script.steps.size()){
        return "went on as at the wall: " + stopped_at;
    }
    const bool replanned = std::any_of(script.notes.begin(), script.notes.end(), [](const std::string& note){
        return note.find("stopped early after about 1 tiles") != std::string::npos && note.find("planning again") != std::string::npos;
    });
    return replanned ? UnitTestResult(true) : UnitTestResult("no replan after the stop a tile short.");
}


//  A leg to a wall 8 tiles away that stops far later than 8 tiles take: the
//  player pushed against someone on the way, and the time counts 18 tiles
//  (lab-B 2026-11-11 21:00, 00.05b: "east 8 (to wall)" stopped after
//  2800 ms against the boy below the Mart, about 5 tiles in, and the route
//  went on from the wall). Not the wall: plan again from every tile of the
//  leg, and a second leg to the wall pins it.
UnitTestResult test_wall_leg_held_up_is_not_at_its_wall(Logger&){
    const StoryWorld corridor = make_world({"###########", "#.........#", "###########"});
    Script script{.steps = {
        walk_step("wall: T: east 8 (to wall)", WalkStop::BLOCKED, 3930ms, 5),
        walk_step("wall: T: east 7 (to wall)", WalkStop::BLOCKED, 1450ms, 1),
        answer_step("dialog within 500 ms", false),
    }};
    ScriptedWalk walk;
    try{
        walk = walk_script(corridor, {1, 1}, {9, 1}, {}, script);
    }catch (const ScriptMismatch& e){
        return e.message;
    }
    if (!walk.failure.empty()){
        return "failed: " + walk.failure;
    }
    if (walk.at != std::vector<TilePos>{{9, 1}}){
        return "did not arrive at (9, 1).";
    }
    const std::string left = script_left(script);
    return left.empty() ? UnitTestResult(true) : UnitTestResult(left);
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
        .input = {"a person a tile ahead on a short leg is waited out", test_short_leg_waits_for_the_way},
        .expectation = {},
    },
    {
        .input = {"a short leg to a wall a tile short is a stop, not the wall", test_short_leg_a_tile_short_is_not_arrived},
        .expectation = {},
    },
    {
        .input = {"a wall leg that never moved plans from its start", test_never_moved_plans_from_the_start},
        .expectation = {},
    },
    {
        .input = {"a wall leg held up on the way is not at its wall (lab-B 2026-11-11 21:00)", test_wall_leg_held_up_is_not_at_its_wall},
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
