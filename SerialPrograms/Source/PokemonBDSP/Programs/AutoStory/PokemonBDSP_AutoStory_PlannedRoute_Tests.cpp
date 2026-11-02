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
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "Utils/PokemonBDSP_AutoStory_RouteIO.h"
#include "PokemonBDSP_AutoStory_PlannedRoute_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
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
    const char* kind =
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


//  r54: a trainer's line froze the check walk at a leg's wall; after the
//  battle the walk on reached the wall, but the check still judged the
//  frozen walk's DIALOG and failed. The walk on is what counts.
class Test_WallLegTrainerOnTheWay : public UnitTest{
public:
    Test_WallLegTrainerOnTheWay()
        : UnitTest("PokemonBDSP::AutoStory - Planned route: a trainer on a wall leg, then on to the wall")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
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
};


//  A leg to a wall that ran out of time: a check walk that also runs out
//  of time counts as at the wall only if holding on there changes the
//  screen less than walking (r127a ran along the wrong row).
struct WallCheckCase{
    std::string name;
    std::vector<ScriptStep> check;
    bool fails;
};

class Test_WallLegLimit : public UnitTest{
public:
    Test_WallLegLimit()
        : UnitTest("PokemonBDSP::AutoStory - Planned route: a wall leg out of time needs the holding check")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
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
};


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
class Test_OpenLegKnownTrainer : public UnitTest{
public:
    Test_OpenLegKnownTrainer()
        : UnitTest("PokemonBDSP::AutoStory - Planned route: a known trainer stops an open leg")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
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
};

}


void add_tests_AutoStory_PlannedRoute(UnitTestDatabase& database){
    database.add<Test_WallLegTrainerOnTheWay>();
    database.add<Test_WallLegLimit>();
    database.add<Test_OpenLegKnownTrainer>();
}



}
}
}
