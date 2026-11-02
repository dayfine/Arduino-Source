/*  BDSP AutoStory - Story table Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryVariants.h"
#include "PokemonBDSP_AutoStory_Tries.h"
#include "PokemonBDSP_AutoStory_StoryTable_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;

//  Each way of `Way`: its name reads back as itself, names are distinct,
//  and the usual way is "".
template <typename Way>
std::string check_ways(const char* type){
    std::set<std::string> names;
    for (Way way : all_variants<Way>()){
        const std::string name = variant_name(way);
        if (parse_variant<Way>(name) != way || !names.insert(name).second){
            return std::format("{} \"{}\" does not read back as itself.", type, name);
        }
    }
    if (!names.contains("") || parse_variant<Way>("no_such_way")){
        return std::format("{}: no usual way \"\", or an unknown name read as a way.", type);
    }
    return "";
}

//  The typed ways each kind does: names round trip, and the table's lint
//  takes exactly them.
class Test_StoryVariants : public UnitTest{
public:
    Test_StoryVariants()
        : UnitTest("PokemonBDSP::AutoStory - Story step ways: names round trip")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        for (const std::string& problem : {
            check_ways<RouteCosts>("RouteCosts"), check_ways<LegsWay>("LegsWay"), check_ways<MashWay>("MashWay"),
            check_ways<StepWay>("StepWay"), check_ways<TalkWay>("TalkWay"), check_ways<BattleFormat>("BattleFormat"),
        }){
            if (!problem.empty()){
                return problem;
            }
        }
        if (!built_in_variant(StoryStepKind::LEGS, "hold_taps") || built_in_variant(StoryStepKind::LEGS, "doubles") ||
            !built_in_variant(StoryStepKind::MASH, "wait_out") || built_in_variant(StoryStepKind::GAME, "") ||
            !built_in_variant(StoryStepKind::HEAL, "") || built_in_variant(StoryStepKind::HEAL, "a")){
            return "built_in_variant() takes the wrong ways.";
        }
        if (MASH_WAIT_OUT_PRESSES != 8){
            return "MASH_WAIT_OUT_PRESSES is the 12 s (8 presses 1.5 s apart) the president took on the console.";
        }
        if (!built_in_variant(StoryStepKind::TALK, "facing") || TALK_FACING_TRIES != 2){
            return "TALK \"facing\" is a way, with 2 A presses: Barry's two tries.";
        }
        return true;
    }
};

//  What a run of try_bounded() did: the attempts made and the safe moves
//  between them.
struct TriesRun{
    std::optional<size_t> took;
    std::vector<size_t> attempts;
    std::vector<size_t> moves;
};
TriesRun run_tries(size_t tries, std::optional<size_t> takes_at){
    TriesRun run;
    run.took = try_bounded(tries,
        [&](size_t attempt){
            run.attempts.push_back(attempt);
            return takes_at == attempt;
        },
        [&](size_t attempt){ run.moves.push_back(attempt); }
    );
    return run;
}

//  The bound every tolerant step leans on: never more tries than allowed,
//  a safe move only between two, none after the last, and the first try
//  that holds ends it.
class Test_BoundedTries : public UnitTest{
public:
    Test_BoundedTries()
        : UnitTest("PokemonBDSP::AutoStory - Bounded tries: at most N, moves only between")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        struct Case{
            size_t tries;
            std::optional<size_t> takes_at;
            std::optional<size_t> took;
            std::vector<size_t> attempts;
            std::vector<size_t> moves;
        };
        const std::vector<Case> cases{
            {.tries = 3, .takes_at = 1, .took = 1, .attempts = {1}, .moves = {}},
            {.tries = 3, .takes_at = 3, .took = 3, .attempts = {1, 2, 3}, .moves = {1, 2}},
            {.tries = 3, .takes_at = std::nullopt, .took = std::nullopt, .attempts = {1, 2, 3}, .moves = {1, 2}},
            {.tries = 2, .takes_at = 5, .took = std::nullopt, .attempts = {1, 2}, .moves = {1}},
            {.tries = 0, .takes_at = 1, .took = std::nullopt, .attempts = {}, .moves = {}},
        };
        for (const Case& c : cases){
            const TriesRun run = run_tries(c.tries, c.takes_at);
            if (run.took != c.took || run.attempts != c.attempts || run.moves != c.moves){
                return std::format("{} tries, taking at {}: expected {} attempts and {} moves, got {} and {}.",
                    c.tries, c.takes_at.value_or(0), c.attempts.size(), c.moves.size(), run.attempts.size(), run.moves.size());
            }
        }
        return true;
    }
};

//  The table: unique ids in order, each row lints, and every step's variant
//  is one its kind does or one of the story's events.
class Test_StoryTableLints : public UnitTest{
public:
    Test_StoryTableLints()
        : UnitTest("PokemonBDSP::AutoStory - Story table: rows lint, variants known")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const StoryEvents& events = story_events();
        std::set<std::string> ids;
        std::string previous;
        for (const StoryCheckpoint& row : story_checkpoints()){
            if (!ids.insert(row.id).second || row.id <= previous){
                return row.id + ": repeated or out of order.";
            }
            previous = row.id;
            const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(row);
            if (!problems.empty()){
                return row.id + ": " + problems[0];
            }
            for (const Pokemon::StoryStep& step : row.steps){
                const bool known = events.contains(step.variant) ||
                    built_in_variant(step.kind, step.variant);
                if (!known){
                    return std::format("{} {}: no event or {} way named \"{}\".",
                        row.id, step.name, Pokemon::story_step_kind_name(step.kind), step.variant);
                }
            }
        }
        if (!ids.contains("00.00") || !ids.contains("01.11") || story_checkpoints().front().on_failure != StoryFailure::STOP){
            return "Expected 00.00 (which stops on a failure) to 01.11.";
        }
        return true;
    }
};

struct LadyLeg{
    Direction direction;
    size_t tiles;           //  0: walk until the wall
    TilePos end;            //  world tile
    std::string name;
};

//  00.04c's Potions: the Mart lady's walk (mart_lady()) is walks until
//  stopped, so on the map each leg ends at its wall: from a tile to three
//  below the Center's door, a tap east and north to the Center's front,
//  west to the Lab's sign, a row south and west to her grass's edge, north
//  to the trees on her row (she stands east of there), back a row south and
//  east to Route 201's sign, a row south and east to the trees east of the
//  Mart, and north to the front row's end.
class Test_MartLadyWalk : public UnitTest{
public:
    Test_MartLadyWalk()
        : UnitTest("PokemonBDSP::AutoStory - 00.04: the walk to the Poke Mart lady and back")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const StoryWorld& maps = story_maps();
        const auto sandgem = [&](const TilePos& pos){ return maps.to_world(MAP_SANDGEM_TOWN, pos); };
        const auto route201 = [&](const TilePos& pos){ return maps.to_world(MAP_ROUTE_201, pos); };
        const std::vector<LadyLeg> legs{
            {Direction::EAST, 1, sandgem({18, 0}), "a tap east under the Center's wall"},
            {Direction::NORTH, 0, sandgem({18, 11}), "north to the Center's front"},
            {Direction::WEST, 0, sandgem({6, 11}), "west to the Lab's sign"},
            {Direction::SOUTH, 1, sandgem({6, 12}), "a row south onto the path"},
            {Direction::WEST, 0, route201({42, 5}), "west to her grass's edge"},
            {Direction::NORTH, 0, route201({42, 3}), "north to the trees, on her row"},
            {Direction::SOUTH, 1, route201({42, 4}), "a row south onto the signs' row"},
            {Direction::EAST, 0, route201(map_point("Route201", "border_sign").next(Direction::WEST)), "east to Route 201's sign"},
            {Direction::SOUTH, 1, route201({54, 5}), "a row south onto the path"},
            {Direction::EAST, 0, sandgem({29, 12}), "east to the trees east of the Mart"},
            {Direction::NORTH, 0, sandgem(map_point("SandgemTown", "front_row_east_end")), "north to the front row's end"},
        };
        //  Out of the Center's door we stand a tile to three below it (r261).
        for (int below = 1; below <= 3; below++){
            const std::string error = follow(maps, legs, sandgem({17, 10 + below}), logger);
            if (!error.empty()){
                return std::format("From {} below the door: {}", below, error);
            }
        }
        return true;
    }

private:
    static std::string follow(const StoryWorld& maps, const std::vector<LadyLeg>& legs, TilePos at, Logger& logger){
        for (const LadyLeg& leg : legs){
            if (leg.tiles == 0){
                at = slide_to_wall(maps, at, leg.direction).stop;
            }else{
                const std::optional<TilePos> next = PokemonBDSP::step(maps, at, leg.direction);
                if (!next){
                    return leg.name + ": blocked at " + describe(maps, at) + ".";
                }
                at = *next;
            }
            //  The tap east lands on whichever row we stood on.
            if (&leg != &legs.front() && !(at == leg.end)){
                return leg.name + ": ends at " + describe(maps, at) + ", not " + describe(maps, leg.end) + ".";
            }
            logger.log(leg.name + ": " + describe(maps, at));
        }
        return "";
    }
};

//  01.10 on Route 204's north half: the line a human walked (2026-10-31),
//  corner to corner, from the Ravaged Path's exit into Floaroma. Each
//  stretch must plan as the one straight tap leg the human took, on the map
//  as it is: a detour here means the map or the line changed.
class Test_Route204NorthLine : public UnitTest{
public:
    Test_Route204NorthLine()
        : UnitTest("PokemonBDSP::AutoStory - Route 204 North: the line into Floaroma")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const StoryWorld& route = route204_north_maps();
        const std::vector<TilePos> waypoints = map_region("Route204North", "floaroma_waypoints");
        struct Expected{ Direction direction; size_t tiles; };
        const std::vector<Expected> expected{
            {Direction::SOUTH, 1}, {Direction::WEST, 9}, {Direction::NORTH, 5}, {Direction::EAST, 8},
            {Direction::NORTH, 10}, {Direction::WEST, 8}, {Direction::NORTH, 7},
        };
        if (waypoints.size() != expected.size()){
            return std::format("Route 204 North: {} waypoints, expected {}.", waypoints.size(), expected.size());
        }
        std::vector<TilePos> at{map_point("Route204North", "cave_exit")};
        for (size_t i = 0; i < waypoints.size(); i++){
            const TilePos goal = waypoints[i];
            const std::string name = std::format("Route 204 North stretch {} to {}", i + 1, describe(route, goal));
            const auto plan = plan_robust_route(route, at, [&](const TilePos& pos){ return pos == goal; }, waypoint_plan_costs());
            if (!plan){
                return name + ": no plan.";
            }
            if (plan->size() != 1){
                return std::format("{}: {} legs, expected the one straight leg.", name, plan->size());
            }
            const TileLeg& leg = (*plan)[0].leg;
            logger.log(name + ": " + to_string(leg));
            if (leg.ends_at_wall || leg.direction != expected[i].direction || leg.tiles != expected[i].tiles){
                return std::format("{}: planned {}, expected an open leg of {} tiles.", name, to_string(leg), expected[i].tiles);
            }
            if ((*plan)[0].after.size() != 1 || !((*plan)[0].after[0] == goal)){
                return name + ": the leg does not end exactly on the waypoint.";
            }
            at = {goal};
        }
        if (!in_floaroma_path(at[0])){
            return "Route 204 North: the last waypoint is not in the path into Floaroma.";
        }
        return true;
    }
};

}


void add_tests_AutoStory_StoryTable(UnitTestDatabase& database){
    database.add<Test_StoryVariants>();
    database.add<Test_BoundedTries>();
    database.add<Test_StoryTableLints>();
    database.add<Test_Route204NorthLine>();
    database.add<Test_MartLadyWalk>();
}



}
}
}
