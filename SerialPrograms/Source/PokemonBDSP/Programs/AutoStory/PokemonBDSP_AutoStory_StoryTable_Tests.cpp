/*  BDSP AutoStory - Story table Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_TableRoutes.h"
#include "PokemonBDSP_AutoStory.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryTable_Tests.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;
using Pokemon::story_arg;

//  The actions each kind does itself, besides the story's events.
const std::map<StoryStepKind, std::set<std::string>>& built_in_actions(){
    static const std::map<StoryStepKind, std::set<std::string>> actions{
        {StoryStepKind::ROUTE, {"", "route", "town", "robust", "line", "exit", std::string(ROUTE_ONTO_TRIGGER), std::string(ROUTE_INTO_SCENE)}},
        {StoryStepKind::LEGS, {"", "legs", "one_short_ok", "to_wall", "to_dialogue", "to_map", "counted_wall", "measured"}},
        {StoryStepKind::STEP, {"", "face", "to_banner", "tap", "or_legs", "door"}},        {StoryStepKind::MASH, {"", "a"}},
        {StoryStepKind::BATTLE, {"", "fight", "doubles", "starter"}},
        {StoryStepKind::TALK, {"", "talk_to_wanderer", "wait_for_wanderer"}},
        {StoryStepKind::SHOP, {"", "sandgem_potions"}},
    };
    return actions;
}

//  The table: unique ids in order, each row lints, and every step's variant
//  is one its kind does or one of the story's events.
UnitTestResult test_story_table_lints(Logger&){
    const StoryEvents& events = story_events();
    std::set<std::string> ids;
    std::string previous;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (!ids.insert(row.id).second || row.id <= previous){
            return row.id + ": repeated or out of order.";
        }
        previous = row.id;
        if ((row.on_failure == StoryFailure::RELOAD_AND_RETRY) == row.saves_in.empty()){
            return row.id + ": a row names the map its start save is made in (saves_in) exactly when it saves first.";
        }
        const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(row);
        if (!problems.empty()){
            return row.id + ": " + problems[0];
        }
        for (const Pokemon::StoryStep& step : row.steps){
            const auto built_in = built_in_actions().find(step.kind);
            const bool known = events.contains(step.action) ||
                (step.kind != StoryStepKind::GAME && step.action.empty()) ||
                (built_in != built_in_actions().end() && built_in->second.contains(step.action));
            if (!known){
                return std::format("{} {}: no event or {} way named \"{}\".",
                    row.id, step.name, Pokemon::story_step_kind_name(step.kind), step.action);
            }
        }
    }
    if (!ids.contains("00.00") || !ids.contains("00.05c") || story_checkpoints().front().on_failure != StoryFailure::STOP){
        return "Expected 00.00 (which stops on a failure) to 00.05c.";
    }
    return true;
}

//  Where a row's last step says it leaves the player: the end of its scene,
//  or where its route (or heal) goes. None for a step that does not say.
std::optional<Pokemon::Pose> end_pose(const StoryCheckpoint& row){
    if (row.steps.empty()){
        return std::nullopt;
    }
    const Pokemon::StoryStep& last = row.steps.back();
    if (last.pose_after){
        return Pokemon::pose_of(*last.pose_after);
    }
    if (last.kind == StoryStepKind::ROUTE){
        return Pokemon::pose_of(story_arg<Pokemon::GridPose>(last.args, "target"));
    }
    return std::nullopt;
}

//  Each row starts where the row before it ends: every tile its start allows
//  is one the last row's end allows, on the same map. Checked where the last
//  step says where it ends.
UnitTestResult test_story_table_rows_join(Logger&){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    for (size_t i = 1; i < rows.size(); i++){
        const std::optional<Pokemon::Pose> end = end_pose(rows[i - 1]);
        const std::optional<Pokemon::Pose> start = rows[i].start ? std::make_optional(Pokemon::pose_of(*rows[i].start)) : std::nullopt;
        if (!end || !start){
            continue;
        }
        const std::vector<TilePos> ends = Pokemon::tiles_of(*end);
        bool joins = start->map == end->map;
        for (const TilePos& pos : Pokemon::tiles_of(*start)){
            joins = joins && std::find(ends.begin(), ends.end(), pos) != ends.end();
        }
        if (!joins){
            return std::format("{} starts at {}, but {} ends at {}.",
                rows[i].id, Pokemon::to_string(*start), rows[i - 1].id, Pokemon::to_string(*end));
        }
    }
    return true;
}

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
std::string follow_mart_lady(const StoryWorld& maps, const std::vector<LadyLeg>& legs, TilePos at, Logger& logger){
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

UnitTestResult test_mart_lady_walk(Logger& logger){
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
        const std::string error = follow_mart_lady(maps, legs, sandgem({17, 10 + below}), logger);
        if (!error.empty()){
            return std::format("From {} below the door: {}", below, error);
        }
    }
    return true;
}

//  01.10 on Route 204's north half: the line a human walked (2026-10-31),
//  corner to corner, from the Ravaged Path's exit into Floaroma. Each
//  stretch must plan as the one straight tap leg the human took, on the map
//  as it is: a detour here means the map or the line changed.
UnitTestResult test_route204_north_line(Logger& logger){
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

//  01.10's line 5, "north 10 (open)" to (19, 12), after Aroma Lady Taylor's
//  battle (lab-B 2026-11-08 18:15:47): we may stand anywhere on (19, 12..20).
//  Column 19 is open floor from row 7 to 13 below the wall on row 6, so north
//  to that wall and 5 south reach the goal; the console's plan, "south 12 (to
//  wall) to (19, 24), north 12 (open) to (19, 12)", pinned the wall with the
//  fewest held tiles and walked back twice as far.
UnitTestResult test_line_5_recovery_pins_the_wall_nearest_the_goal(Logger& logger){
    const StoryWorld& world = *story_plan_world("Route204North", "Route204North");
    std::vector<TilePos> from;
    for (int y = 12; y <= 20; y++){
        from.push_back(story_to_world(world, "Route204North", {19, y}));
    }
    const TilePos goal = story_to_world(world, "Route204North", {19, 12});
    const auto legs = plan_recovered_route(world, from, [&](const TilePos& pos){ return pos == goal; }, *story_route_costs("line"));
    if (!legs || legs->empty()){
        return "no plan from (19, 12..20).";
    }
    for (const PlannedLeg& planned : *legs){
        logger.log("01.10 line 5 recovery: " + to_string(planned));
    }
    const PlannedLeg& first = legs->front();
    const TilePos wall = story_to_world(world, "Route204North", {19, 7});
    if (first.leg.direction != Direction::NORTH || !first.leg.ends_at_wall || first.after != std::vector<TilePos>{wall}){
        return "the recovery starts " + to_string(first) + ", not north to the wall at (19, 7).";
    }
    return true;
}

//  Why `planned` may slide into a door or a warp, or "": an open leg (not to
//  a wall) that may end on the tile before one, walking toward it. A part of
//  a tile too far takes the warp (lab-B 2026-11-08 15:31 to 15:54: 01.05's
//  open walk west to (2, 13), beside the Oreburgh Gate's mouth, slid into
//  the Gate, and the tap after it waited for a banner already gone). The
//  maps draw every warp as a door tile, cave mouths too.
std::string slides_into_door(const StoryWorld& world, const PlannedLeg& planned){
    if (planned.leg.ends_at_wall){
        return "";
    }
    for (const TilePos& pos : planned.after){
        const TilePos ahead = pos.next(planned.leg.direction);
        if (world.tile(ahead) == Pokemon::TileKind::DOOR){
            return std::format("the open leg {} may end on {}, the tile before the door {}",
                to_string(planned.leg), describe(world, pos), describe(world, ahead));
        }
    }
    return "";
}

//  A ROUTE step of the table from a known place: the world it plans on and
//  its plan, or nothing if it finds none.
struct PlannedStoryRoute{
    std::string name;       //  "<row id> <step name>"
    const StoryWorld* world;
    std::vector<TilePos> from;
    std::optional<std::vector<PlannedLeg>> legs;
};

//  Every ROUTE step of the table that starts from a known place, planned as
//  walk_route() plans it. Where a row's steps leave the player is followed
//  from its start through each step's target or pose_after; a route from an
//  unknown place is logged, not planned.
std::vector<PlannedStoryRoute> plan_story_routes(Logger& logger){
    std::vector<PlannedStoryRoute> routes;
    for (const TableRoute& route : table_routes()){
        if (route.world == nullptr){
            logger.log(std::format("{} {}: not checked, from {}.", route.row, route.step->name,
                route.from ? Pokemon::to_string(*route.from) : "nowhere known"));
            continue;
        }
        RouteStepPlan plan = plan_table_route(route);
        routes.push_back({route.row + " " + route.step->name, route.world, std::move(plan.start), std::move(plan.legs)});
    }
    return routes;
}

//  No ROUTE step of the table plans an open leg that ends before a door it
//  walks toward: a door is gone through by a step that runs or taps into it
//  from where the route ends on a wall or off its line.
UnitTestResult test_story_routes_stop_short_of_doors(Logger& logger){
    std::vector<std::string> found;
    size_t checked = 0;
    for (const PlannedStoryRoute& route : plan_story_routes(logger)){
        checked += route.legs ? 1 : 0;
        for (const PlannedLeg& planned : route.legs ? *route.legs : std::vector<PlannedLeg>{}){
            const std::string why = slides_into_door(*route.world, planned);
            if (!why.empty()){
                found.push_back(std::format("{}: {}.", route.name, why));
            }
        }
    }
    logger.log(std::format("{} routes planned.", checked));
    std::string out;
    for (const std::string& line : found){
        out += "\n  " + line;
    }
    return found.empty() ? UnitTestResult(true) : UnitTestResult("Open legs that end before a door they walk toward:" + out);
}

//  The tiles a planned leg walks over from `from`, `from` first: to the wall,
//  or its tiles' worth of steps.
std::vector<TilePos> walked_tiles(const StoryWorld& world, const TilePos& from, const TileLeg& leg){
    std::vector<TilePos> tiles{from};
    size_t walked = 0;
    while (leg.ends_at_wall || walked < leg.tiles){
        const std::optional<TilePos> next = step(world, tiles.back(), leg.direction);
        if (!next){
            break;
        }
        walked += (size_t)(std::abs(next->x - tiles.back().x) + std::abs(next->y - tiles.back().y));
        tiles.push_back(*next);
    }
    return tiles;
}

//  The most tiles of `zone` one leg of `legs` walks over, from any tile the
//  player may be on, and that leg.
std::pair<size_t, std::string> most_zone_tiles(
    const StoryWorld& world, std::vector<TilePos> from, const std::vector<PlannedLeg>& legs,
    const std::function<bool(const TilePos&)>& zone
){
    std::pair<size_t, std::string> most{0, ""};
    for (const PlannedLeg& planned : legs){
        for (const TilePos& pos : from){
            const std::vector<TilePos> tiles = walked_tiles(world, pos, planned.leg);
            const size_t in_zone = (size_t)std::count_if(tiles.begin(), tiles.end(), zone);
            if (in_zone > most.first){
                most = {in_zone, to_string(planned.leg) + " from " + describe(world, pos)};
            }
        }
        from = planned.after;
    }
    return most;
}

//  Where a planned leg from `from` runs along a zone where someone wanders,
//  or "": two zone tiles in a row on a straight leg are tiles of one zone.
std::string along_wander_zone(const StoryWorld& world, const TilePos& from, const TileLeg& leg){
    const std::vector<TilePos> tiles = walked_tiles(world, from, leg);
    for (size_t i = 1; i < tiles.size(); i++){
        if (wandering_hazard(world, tiles[i - 1]) && wandering_hazard(world, tiles[i])){
            return std::format("{} from {} runs along a wander zone at {}", to_string(leg), describe(world, from), describe(world, tiles[i]));
        }
    }
    return "";
}

//  Every ROUTE step of the table from a known place has a plan, and no leg
//  of it runs along a zone where someone wanders: plan_robust_route() never
//  plans one, so a step with no other way finds no plan here.
UnitTestResult test_story_routes_keep_off_wander_zones(Logger& logger){
    std::string out;
    for (const PlannedStoryRoute& route : plan_story_routes(logger)){
        if (!route.legs){
            out += "\n  " + route.name + ": no plan.";
            continue;
        }
        std::vector<TilePos> from = route.from;
        for (const PlannedLeg& planned : *route.legs){
            for (const TilePos& pos : from){
                const std::string along = along_wander_zone(*route.world, pos, planned.leg);
                out += along.empty() ? "" : "\n  " + route.name + ": " + along + ".";
            }
            from = planned.after;
        }
    }
    return out.empty() ? UnitTestResult(true) : UnitTestResult("ROUTE steps:" + out);
}

//  No leg of 00.05b's or 00.05c's routes enters a tile where someone
//  wanders, from any tile the player may be on: on lab-B (2026-11-11 21:00)
//  the boy below the Mart (51, 33..35) stopped 00.05b's "east 8 (to wall)"
//  along row 34 three tiles short; row 36 below him is open. The Mart
//  clown's box counts too: the routes reach column 56 and leave it at
//  (56, 36) and (56, 31), outside it, and wait_for_wanderer walks the
//  column between.
UnitTestResult test_jubilife_routes_cross_no_wander_zone(Logger& logger){
    std::string out;
    size_t checked = 0;
    for (const PlannedStoryRoute& route : plan_story_routes(logger)){
        if (!route.name.starts_with("00.05b ") && !route.name.starts_with("00.05c ")){
            continue;
        }
        if (!route.legs){
            out += "\n  " + route.name + ": no plan.";
            continue;
        }
        checked++;
        const StoryWorld& world = *route.world;
        const Pokemon::TileBox clown = jubilife_mart_clown_box();
        const auto [most, leg] = most_zone_tiles(world, route.from, *route.legs, [&](const TilePos& pos){
            const std::optional<TilePos> local = story_to_local(world, "JubilifeCity", pos);
            const bool clowns = local && local->x >= clown.x0 && local->x <= clown.x1 && local->y >= clown.y0 && local->y <= clown.y1;
            return clowns || wandering_hazard(world, pos);
        });
        if (most > 0){
            out += std::format("\n  {}: {} enters {} tile(s) where someone wanders.", route.name, leg, most);
        }
    }
    logger.log(std::format("{} Jubilife routes checked.", checked));
    if (checked == 0){
        return "No 00.05b or 00.05c route planned from a known place.";
    }
    return out.empty() ? UnitTestResult(true) : UnitTestResult("ROUTE steps:" + out);
}

//  Where the walk from `x` on row 22 walks the plaza, or "".
//  The Wonder Station's plaza on JubilifeCity, as measured on lab-B
//  (2026-11-12): its round wall and the woman by it stop walks west on
//  rows 21 to 24 from x 52 and on row 26 from x 43 (taps from (58, 26)
//  moved 14 tiles clean to (44, 26), then stopped by her). Row 26 east of
//  her is open: the way west goes south round her.
constexpr Pokemon::TileBox WONDER_STATION_PLAZA[] = {
    {.x0 = 40, .y0 = 20, .x1 = 52, .y1 = 25},
    {.x0 = 40, .y0 = 26, .x1 = 43, .y1 = 27},
};

std::string company_clown_plaza_tiles(Logger& logger, TableRoute route, int x){
    route.from = Pokemon::tile_pose("JubilifeCity", {{x, 21}}, Direction::WEST);
    const RouteStepPlan plan = plan_table_route(route);
    if (!plan.legs){
        return std::format("no plan from ({}, 21).", x);
    }
    const StoryWorld& world = *route.world;
    for (const PlannedLeg& planned : *plan.legs){
        logger.log(std::format("00.05c beside_the_president from ({}, 21): {}", x, to_string(planned)));
    }
    const auto plaza = [&](const TilePos& pos){
        const std::optional<TilePos> local = story_to_local(world, "JubilifeCity", pos);
        return local && std::any_of(std::begin(WONDER_STATION_PLAZA), std::end(WONDER_STATION_PLAZA), [&](const Pokemon::TileBox& box){
            return local->x >= box.x0 && local->x <= box.x1 && local->y >= box.y0 && local->y <= box.y1;
        });
    };
    const auto [most, leg] = most_zone_tiles(world, plan.start, *plan.legs, plaza);
    return most == 0 ? "" : std::format("the leg {} walks {} tile(s) of the plaza.", leg, most);
}

//  00.05c's first walk (to the president since its clown re-talks went;
//  to the Poketch Company's clown before) from the street
//  (57..58, 21), where the checkpoint starts on a rerun from 01.01's start,
//  keeps off the plaza (WONDER_STATION_PLAZA). BDSP put the Global Wonder Station
//  there, a round wall with people round it that pret's Diamond map lacks:
//  on lab-B (2026-11-12 01:22 and 01:55) the walk west from (57, 22) was
//  stopped at (50, 22), (51, 21) and (49, 23), and talked to the woman at
//  (45, 24) until the step failed.
UnitTestResult test_company_clown_from_the_street_keeps_off_the_plaza(Logger& logger){
    for (const TableRoute& route : table_routes()){
        if (route.row != "00.05c" || route.step->name != "beside_the_president" || route.world == nullptr){
            continue;
        }
        std::string out;
        for (int x : {57, 58}){
            const std::string found = company_clown_plaza_tiles(logger, route, x);
            out += found.empty() ? "" : "\n  " + found;
        }
        return out.empty() ? UnitTestResult(true) : UnitTestResult("00.05c beside_the_president:" + out);
    }
    return "00.05c beside_the_president: not in the table, or from nowhere known.";
}

//  00.05c's replan on lab-B (2026-11-12 02:52): the walk west on row 24
//  talked to the woman it took for (34, 24), marked her tile off the map,
//  and from (35, 24) and (36, 24), with (43, 26) marked as a wall, found no
//  plan at all, with the route costs or the default ones.
UnitTestResult test_company_clown_after_the_woman_on_row_24(Logger& logger){
    for (TableRoute route : table_routes()){
        if (route.row != "00.05c" || route.step->name != "beside_the_president" || route.world == nullptr){
            continue;
        }
        StoryWorld world = *route.world;
        world.block(story_to_world(world, "JubilifeCity", {43, 26}));
        world.uncover(story_to_world(world, "JubilifeCity", {34, 24}));
        route.world = &world;
        route.from = Pokemon::tile_pose("JubilifeCity", {{35, 24}, {36, 24}}, Direction::WEST);
        const RouteStepPlan plan = plan_table_route(route);
        if (!plan.legs){
            return "00.05c beside_the_president: no plan from (35, 24), (36, 24).";
        }
        for (const PlannedLeg& planned : *plan.legs){
            logger.log("00.05c beside_the_president from (35, 24), (36, 24): " + to_string(planned));
        }
        return UnitTestResult(true);
    }
    return "00.05c beside_the_president: not in the table, or from nowhere known.";
}

//  00.05c's end below the east-exit sign (59, 21) facing north, on the
//  planned route from the Mart's street (lab-B 2026-11-12 13:55: a "measured"
//  walk inferred its start and tapped against the hedges). Each leg is taps
//  (a whole tile each, a turn tap first) or a wall's end, and none steps on
//  the guard's trigger column 60.
UnitTestResult test_main_road_ends_below_the_east_exit_sign(Logger& logger){
    for (const PlannedStoryRoute& route : plan_story_routes(logger)){
        if (route.name != "00.05c below_the_east_exit_sign"){
            continue;
        }
        if (!route.legs || route.legs->empty()){
            return "00.05c below_the_east_exit_sign: no plan.";
        }
        const StoryWorld& world = *route.world;
        const auto guard_column = [&](const TilePos& pos){
            const std::optional<TilePos> local = story_to_local(world, "JubilifeCity", pos);
            return local && local->x >= 60;
        };
        const auto [most, leg] = most_zone_tiles(world, route.from, *route.legs, guard_column);
        if (most > 0){
            return std::format("the leg {} steps on {} tiles of column 60 or east.", leg, most);
        }
        const PlannedLeg& last = route.legs->back();
        const auto on_the_tile = [&](const TilePos& pos){
            const std::optional<TilePos> local = story_to_local(world, "JubilifeCity", pos);
            return local && *local == TilePos{59, 21};
        };
        if (last.after.size() != 1 || !on_the_tile(last.after.front()) || last.leg.direction != Direction::NORTH || to_string(last).find("to wall") == std::string::npos){
            return "00.05c below_the_east_exit_sign does not end on (59, 21) facing north against the sign.";
        }
        return true;
    }
    return "00.05c below_the_east_exit_sign is not a ROUTE step from a known place.";
}

//  A leg whose end has a wall (or the maps' edge) right after it in the
//  leg's direction is a leg to the wall: held until the wall stops the
//  player, which localizes, not tapped to a tile and left open (lab-B,
//  2026-11-12 14:24: 00.05c's "east 14 (open)" to (59, 26) was a run into
//  the wall past it).
UnitTestResult test_legs_ending_at_a_wall_are_wall_legs(Logger& logger){
    std::vector<std::string> found;
    for (const PlannedStoryRoute& route : plan_story_routes(logger)){
        for (const PlannedLeg& planned : route.legs ? *route.legs : std::vector<PlannedLeg>{}){
            const bool at_wall_end = !planned.after.empty() && std::all_of(planned.after.begin(), planned.after.end(), [&](const TilePos& pos){
                return at_wall(*route.world, pos, planned.leg.direction);
            });
            if (!planned.leg.ends_at_wall && at_wall_end){
                found.push_back(std::format("{}: {}.", route.name, to_string(planned)));
            }
        }
    }
    std::string out;
    for (const std::string& line : found){
        out += "\n  " + line;
    }
    return found.empty() ? UnitTestResult(true) : UnitTestResult("open legs that end at a wall:" + out);
}

//  After a tapped leg the walk knows its place exactly: a leg to a wall
//  from it is not followed by a leg straight back (00.05c's "east 19
//  (open)", "south 15 (to wall)", then "north 1"). Run to a wall and back is
//  fine from the start or after a wall (00.04 into_twinleaf, south 37).
UnitTestResult test_no_wall_and_back_after_taps(Logger& logger){
    std::vector<std::string> found;
    for (const PlannedStoryRoute& route : plan_story_routes(logger)){
        if (!route.legs){
            continue;
        }
        const std::vector<PlannedLeg>& legs = *route.legs;
        for (size_t i = 2; i < legs.size(); i++){
            const bool back = (int)legs[i].leg.direction == ((int)legs[i - 1].leg.direction + 2) % 4;
            const TileLeg& taps = legs[i - 2].leg;
            if (!taps.ends_at_wall && !taps.held && legs[i - 1].leg.ends_at_wall && back){
                found.push_back(std::format("{}: {}, {} then {}.", route.name, to_string(legs[i - 2]), to_string(legs[i - 1]), to_string(legs[i])));
            }
        }
    }
    std::string out;
    for (const std::string& line : found){
        out += "\n  " + line;
    }
    return found.empty() ? UnitTestResult(true) : UnitTestResult("a wall and back after a tapped leg:" + out);
}

//  The route plan dump: every table ROUTE step's legs, in a golden file
//  beside the story data, so a planner or table change shows its plans' diff
//  in review.

constexpr std::string_view ROUTE_PLANS_HEADER =
    "BDSP AutoStory: the legs each ROUTE step of the story table plans.\n"
    "Planned as the console's RouteWalker plans them: plan_recovered_route() on\n"
    "story_plan_world(), from the tiles the row's steps before it leave the\n"
    "player on, with the step's costs, then the default costs if those find none.\n"
    "Written by the StoryTable test \"every ROUTE step plans as\n"
    "Data/route_plans.golden.txt says\". After a planner or table change, run\n"
    "that test with PA_UPDATE_ROUTE_PLANS=1 to rewrite this file\n"
    "(PA_ROUTE_PLANS_GOLDEN=<path> reads or writes another copy), and review\n"
    "its diff.\n"
    "turns: direction changes between legs. open: legs that stop in the open\n"
    "(tapped, not held to a wall). localization: the wall holds at the start\n"
    "that only bring several start tiles to one (plan_recovered_route()'s\n"
    "anchor); 0 from one start tile.\n";

std::string tile_list(const StoryWorld& world, const std::vector<TilePos>& tiles){
    std::string out;
    for (const TilePos& pos : tiles){
        out += (out.empty() ? "" : ", ") + describe(world, pos);
    }
    return out;
}

//  plan_recovered_route()'s wall holds from several start tiles: the legs up
//  to where the tiles the player may be on first become one.
size_t localization_legs(const std::vector<TilePos>& start, const std::vector<PlannedLeg>& legs){
    if (start.size() < 2){
        return 0;
    }
    for (size_t i = 0; i < legs.size(); i++){
        if (legs[i].after.size() == 1){
            return i + 1;
        }
    }
    return 0;
}

std::string plan_totals(const RouteStepPlan& plan){
    const std::vector<PlannedLeg>& legs = *plan.legs;
    size_t turns = 0;
    size_t open = 0;
    for (size_t i = 0; i < legs.size(); i++){
        turns += i > 0 && legs[i].leg.direction != legs[i - 1].leg.direction ? 1 : 0;
        open += legs[i].leg.ends_at_wall ? 0 : 1;
    }
    return std::format("  totals: {} legs, {} turns, {} open, {} localization\n",
        legs.size(), turns, open, localization_legs(plan.start, legs));
}

std::string dump_plan(const StoryWorld& world, const RouteStepPlan& plan){
    std::string out = "  start candidates: " + tile_list(world, plan.start) + "\n";
    if (!plan.legs){
        return out + "  NO PLAN\n";
    }
    if (plan.default_costs){
        out += "  the step's costs found no plan; planned with the default costs\n";
    }
    for (size_t i = 0; i < plan.legs->size(); i++){
        const PlannedLeg& planned = (*plan.legs)[i];
        out += std::format("  {}. {} -> {}\n", i + 1, to_string(planned.leg), tile_list(world, planned.after));
    }
    return out + plan_totals(plan);
}

std::string dump_route(const TableRoute& route){
    const Pokemon::StoryStep& step = *route.step;
    std::string out = std::format("== {} {} (costs: {})\n", route.row, step.name, step.action.empty() ? "route" : step.action);
    out += "  from: " + (route.from ? Pokemon::to_string(*route.from) : std::string("unknown")) + "\n";
    const Pokemon::Pose target = Pokemon::pose_of(story_arg<Pokemon::GridPose>(step.args, "target"));
    out += "  target: " + Pokemon::to_string(target) + "\n";
    if (route.world == nullptr){
        return out + (route.from && !Pokemon::tiles_of(*route.from).empty()
            ? "  not planned: no story world joins the two maps\n"
            : "  not planned: the start is unknown (a LEGS or STEP step before it does not say where it ends)\n");
    }
    return out + dump_plan(*route.world, plan_table_route(route));
}

std::string route_plans_text(){
    std::string out = std::string(ROUTE_PLANS_HEADER);
    for (const TableRoute& route : table_routes()){
        out += "\n" + dump_route(route);
    }
    return out;
}

//  The golden file, in the Data folder beside this file's.
std::string route_plans_golden_path(){
    const char* path = std::getenv("PA_ROUTE_PLANS_GOLDEN");
    if (path != nullptr && path[0] != '\0'){
        return path;
    }
    const std::string self = __FILE__;
    const size_t slash = self.find_last_of("/\\");
    return (slash == std::string::npos ? std::string() : self.substr(0, slash + 1)) + "Data/route_plans.golden.txt";
}

//  `text` without carriage returns or a byte order mark.
std::string plain_lines(const std::string& text){
    std::string out = text.starts_with("\xef\xbb\xbf") ? text.substr(3) : text;
    std::erase(out, '\r');
    return out;
}

//  `text` split into the header and one block per "== " step line.
std::vector<std::string> plan_blocks(const std::string& text){
    std::vector<std::string> blocks{""};
    size_t begin = 0;
    while (begin < text.size()){
        const size_t end = std::min(text.find('\n', begin), text.size() - 1) + 1;
        const std::string line = text.substr(begin, end - begin);
        if (line.starts_with("== ")){
            blocks.push_back("");
        }
        blocks.back() += line;
        begin = end;
    }
    return blocks;
}

//  Each step whose block changed, as "-" (golden) and "+" (planned) blocks.
std::string plan_differences(const std::string& golden, const std::string& planned){
    const std::vector<std::string> expected = plan_blocks(golden);
    const std::vector<std::string> actual = plan_blocks(planned);
    std::string out;
    size_t changed = 0;
    for (size_t i = 0; i < std::max(expected.size(), actual.size()); i++){
        const std::string& was = i < expected.size() ? expected[i] : "";
        const std::string& now = i < actual.size() ? actual[i] : "";
        if (was != now && changed++ < 5){
            out += "\n--- golden:\n" + was + "+++ planned:\n" + now;
        }
    }
    return changed == 0 ? "" : std::format("{} block(s) differ, the first ones:{}", changed, out);
}

//  Every table ROUTE step plans the legs the golden file lists.
UnitTestResult test_route_plans_golden(Logger& logger){
    const std::string path = route_plans_golden_path();
    const std::string planned = route_plans_text();
    const char* update = std::getenv("PA_UPDATE_ROUTE_PLANS");
    if (update != nullptr && update[0] != '\0' && update[0] != '0'){
        std::string crlf;
        for (char ch : planned){
            crlf += ch == '\n' ? std::string("\r\n") : std::string(1, ch);
        }
        FileIO file;
        if (!file.open(path, FileMode::WRITE | FileMode::BINARY) || file.write(crlf) != crlf.size()){
            return "Could not write " + path + ".";
        }
        logger.log("Wrote " + path + ".");
        return true;
    }
    std::string golden;
    if (!file_to_string(path, golden)){
        return "Could not read " + path + " (set PA_ROUTE_PLANS_GOLDEN to its path).";
    }
    const std::string differences = plan_differences(plain_lines(golden), planned);
    if (differences.empty()){
        return true;
    }
    return "The route plans differ from " + path + ". If the change is meant, rerun with PA_UPDATE_ROUTE_PLANS=1 "
        "and review the file's diff. " + differences;
}


//  plan_story_run() with Starter RNG handoff: off leaves the range unchanged;
//  on stops after 00.01 with a save at the Verity Lakefront (17, 14) facing
//  north, where 00.02 and Starter RNG start; starting past 00.01 runs on.
UnitTestResult test_plan_story_run_starter_rng(Logger&){
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    if (table.size() < 3){
        return std::format("Expected the story table to reach 00.02, got {} rows.", table.size());
    }
    const StoryRunPlan full = plan_story_run(table, 0, table.size() - 1, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::OFF});
    if (full.stop != StoryHandoff::NONE || full.rows.size() != table.size()){
        return "With Starter RNG handoff off, expected the full table with no stop.";
    }
    const StoryRunPlan handoff = plan_story_run(table, 0, table.size() - 1, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::STOP});
    if (handoff.stop != StoryHandoff::STARTER_RNG || handoff.rows.size() != 2 ||
        handoff.rows[0].id != "00.00" || handoff.rows[1].id != "00.01"
    ){
        return "Expected a 00.00..01.11 run with Starter RNG handoff to stop after 00.01.";
    }
    const StoryCheckpoint& lakefront_row = handoff.rows.back();
    const std::vector<std::string> problems = Pokemon::lint_story_checkpoint(lakefront_row);
    if (!problems.empty()){
        return std::format("00.01 with Starter RNG handoff save: {}", problems[0]);
    }
    if (lakefront_row.steps.empty() || lakefront_row.steps.back().kind != StoryStepKind::SAVE ||
        lakefront_row.steps.back().name != "save_for_starter_rng"
    ){
        return "Expected 00.01 with Starter RNG handoff to end with save_for_starter_rng.";
    }
    //  The save card is checked as every other start save is: it must name
    //  00.02's map.
    if (lakefront_row.steps.back().action != table[2].saves_in || table[2].saves_in.empty()){
        return std::format(
            "Expected the handoff save to check the card for \"{}\", got \"{}\".",
            table[2].saves_in, lakefront_row.steps.back().action
        );
    }
    const std::optional<Pokemon::Pose> end = end_pose(lakefront_row);
    const std::optional<Pokemon::Pose> next_start = table[2].start
        ? std::make_optional(Pokemon::pose_of(*table[2].start))
        : std::nullopt;
    if (!end || !next_start || end->map != "VerityLakefront" ||
        Pokemon::to_string(*end) != Pokemon::to_string(*next_start)
    ){
        return "Expected 00.01's Starter RNG handoff save to end at 00.02's VerityLakefront (17, 14) start pose.";
    }
    const StoryRunPlan from_0002 = plan_story_run(table, 2, 4, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::STOP});
    if (from_0002.stop != StoryHandoff::NONE || from_0002.rows.size() != 3 || from_0002.rows.front().id != "00.02"){
        return "Expected a run starting at 00.02 to pass through without stopping for 00.01's handoff.";
    }
    return true;
}

//  plan_story_run() with Run Starter RNG in AutoStory: 00.02 runs the
//  starter_rng event after its Verity Lakefront start save and ends where
//  00.03 starts; the handoff stop is not taken.
UnitTestResult test_plan_story_run_in_program(Logger&){
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    if (table.size() < 4){
        return std::format("Expected the story table to reach 00.03, got {} rows.", table.size());
    }
    const StoryRunPlan in_program = plan_story_run(
        table, 0, table.size() - 1, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::RUN}
    );
    if (in_program.stop != StoryHandoff::NONE || in_program.rows.size() != table.size()){
        return "With Run Starter RNG in AutoStory on, expected the full table with no handoff stop.";
    }
    const StoryCheckpoint& rng_row = in_program.rows[2];
    const std::vector<std::string> rng_problems = Pokemon::lint_story_checkpoint(rng_row);
    if (!rng_problems.empty()){
        return std::format("00.02 with Run Starter RNG in AutoStory: {}", rng_problems[0]);
    }
    if (rng_row.id != "00.02" || rng_row.saves_in != "Verity Lakefront" || rng_row.steps.size() != 1 ||
        rng_row.steps[0].kind != StoryStepKind::GAME || rng_row.steps[0].action != "starter_rng" ||
        !story_events().contains(rng_row.steps[0].action)
    ){
        return "Expected 00.02 with Run Starter RNG in AutoStory to run the starter_rng game event after its Verity Lakefront start save.";
    }
    const std::optional<Pokemon::Pose> rng_end = end_pose(rng_row);
    const std::optional<Pokemon::Pose> row0003_start = table[3].start
        ? std::make_optional(Pokemon::pose_of(*table[3].start))
        : std::nullopt;
    if (!rng_end || !row0003_start || Pokemon::to_string(*rng_end) != Pokemon::to_string(*row0003_start)){
        return "Expected 00.02's starter_rng step to end at 00.03's VerityLakefront (17, 14..15) start pose.";
    }
    if (bdsp_starter_for_choice(StarterChoice::TURTWIG) != BdspStarter::Turtwig ||
        bdsp_starter_for_choice(StarterChoice::CHIMCHAR) != BdspStarter::Chimchar ||
        bdsp_starter_for_choice(StarterChoice::PIPLUP) != BdspStarter::Piplup
    ){
        return "Expected StarterChoice to map 1-to-1 to BdspStarter.";
    }
    return true;
}


//  The Starter RNG run's settings: the player's model, the starter and the
//  reset limit come from AutoStory's options, and the run ends where 00.03
//  starts (Lakefront overworld, menu closed).
UnitTestResult test_starter_rng_settings(Logger&){
    BdspRngFilterTable table("Filters", Pokemon::StatsHuntMiscFeatureFlags{false, true, true, true});
    set_default_shiny_row(table);
    const BdspRngFilterSnapshot filters = table.make_snapshot();
    const StoryStarterRngConfig config{
        .language = Language::French, .player_model = 5, .starter = StarterChoice::PIPLUP, .max_resets = 7
    };
    const StarterRngSettings settings = make_starter_rng_settings(config, filters);
    if (settings.player_model != 5){
        return std::format("Expected the story's player model 5 to reach Starter RNG, got {}.", settings.player_model);
    }
    if (settings.starter != BdspStarter::Piplup || settings.language != Language::French || settings.max_resets != 7){
        return "Expected the starter, language and reset limit to come from the story's options.";
    }
    if (settings.filters != &filters || settings.start_point != BdspStartPoint::Lakefront){
        return "Expected the filter snapshot and the Verity Lakefront start point.";
    }
    if (settings.reset_first || !settings.finish_battle_on_shiny || !settings.close_summary_after_read){
        return "Expected a run that starts from the loaded save and leaves the Lakefront overworld on a shiny or not.";
    }
    return true;
}

//  What Starter RNG ending means for the story: a starter in hand and the
//  game on the Lakefront lets it go on; anything else stops it with a reason.
UnitTestResult test_starter_rng_failure(Logger&){
    StarterRngRunResult result;
    result.outcome = StarterRngOutcome::HIT;
    if (!starter_rng_failure(result).empty()){
        return "Expected a hit to let the story go on.";
    }
    result.outcome = StarterRngOutcome::SHINY;
    if (!starter_rng_failure(result).empty()){
        return "Expected a settled shiny to let the story go on.";
    }
    result.settled = false;
    if (starter_rng_failure(result).empty()){
        return "Expected a shiny that did not settle (the game is mid-scene) to stop the story.";
    }
    for (const StarterRngOutcome outcome : {StarterRngOutcome::ABANDONED, StarterRngOutcome::OUT_OF_RESETS}){
        StarterRngRunResult failed;
        failed.outcome = outcome;
        failed.resets_used = 3;
        if (starter_rng_failure(failed).empty()){
            return "Expected a run with no starter to stop the story.";
        }
    }
    return true;
}

//  A failed Run Starter RNG ends the program: 00.02's row retry would rerun
//  the whole reset loop. Without it no row is fatal on top of the table's.
UnitTestResult test_starter_rng_row_is_fatal(Logger&){
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    const StoryRunPlan in_program = plan_story_run(
        table, 0, 3, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::RUN}
    );
    if (in_program.fatal_rows != std::set<std::string>{"00.02"}){
        return "Expected 00.02 to be fatal when Run Starter RNG in AutoStory is on.";
    }
    const StoryRunPlan plain = plan_story_run(table, 0, 3, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::STOP});
    const StoryRunPlan off = plan_story_run(table, 0, 3);
    if (!plain.fatal_rows.empty() || !off.fatal_rows.empty()){
        return "Expected no fatal rows unless Run Starter RNG in AutoStory is on.";
    }
    return true;
}


struct AutoStoryStoryTableInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryStoryTableExpectation{
};

struct AutoStoryStoryTableCase{
    AutoStoryStoryTableInput input;
    AutoStoryStoryTableExpectation expectation;
};

class Test_AutoStoryStoryTable : public UnitTest{
public:
    explicit Test_AutoStoryStoryTable(AutoStoryStoryTableCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - " + test_case.input.name)
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
    AutoStoryStoryTableCase m_case;
};

} // namespace


const AutoStoryStoryTableCase AUTO_STORY_STORY_TABLE_CASES[] = {
    {
        .input = {"Story table: rows lint, variants known", test_story_table_lints},
        .expectation = {},
    },
    {
        .input = {"Story table: each row starts where the last one ends", test_story_table_rows_join},
        .expectation = {},
    },
    {
        .input = {"00.04: the walk to the Poke Mart lady and back", test_mart_lady_walk},
        .expectation = {},
    },
    {
        .input = {"Route 204 North: the line into Floaroma", test_route204_north_line},
        .expectation = {},
    },
    {
        .input = {"01.10: line 5's recovery pins the wall nearest its goal (lab-B, 2026-11-08)", test_line_5_recovery_pins_the_wall_nearest_the_goal},
        .expectation = {},
    },
    {
        .input = {"Story table: no open leg ends before a door it walks toward (lab-B, 2026-11-08)", test_story_routes_stop_short_of_doors},
        .expectation = {},
    },
    {
        .input = {"Story table: every ROUTE step plans as Data/route_plans.golden.txt says", test_route_plans_golden},
        .expectation = {},
    },
    {
        .input = {"Story table: every route plans, and none runs along a wander zone", test_story_routes_keep_off_wander_zones},
        .expectation = {},
    },
    {
        .input = {"00.05c: the plan to below the east-exit sign is taps and a wall's end, off the guard's column (lab-B, 2026-11-12 13:55)", test_main_road_ends_below_the_east_exit_sign},
        .expectation = {},
    },
    {
        .input = {"a leg that ends at a wall is planned as a leg to the wall (00.05c east 14 to (59, 26), lab-B 2026-11-12 14:24)", test_legs_ending_at_a_wall_are_wall_legs},
        .expectation = {},
    },
    {
        .input = {"no run to a wall and back after a tapped leg (00.05c east 19, south 15, north 1, lab-B 2026-11-12 14:24)", test_no_wall_and_back_after_taps},
        .expectation = {},
    },
    {
        .input = {"00.05b, 00.05c: no leg enters a wander zone (lab-B, 2026-11-11)", test_jubilife_routes_cross_no_wander_zone},
        .expectation = {},
    },
    {
        .input = {"00.05c: from the street (57..58, 21), the walk keeps off the Wonder Station's plaza (lab-B, 2026-11-12)", test_company_clown_from_the_street_keeps_off_the_plaza},
        .expectation = {},
    },
    {
        .input = {"00.05c: a plan from (35, 24), (36, 24) past the woman on row 24 (lab-B, 2026-11-12 02:52)", test_company_clown_after_the_woman_on_row_24},
        .expectation = {},
    },
    {
        .input = {"Starter RNG handoff: stops after 00.01 saved at the Verity Lakefront", test_plan_story_run_starter_rng},
        .expectation = {},
    },
    {
        .input = {"Run Starter RNG: 00.02 runs the starter_rng event", test_plan_story_run_in_program},
        .expectation = {},
    },
    {
        .input = {"Run Starter RNG: settings come from the story's options", test_starter_rng_settings},
        .expectation = {},
    },
    {
        .input = {"Run Starter RNG: what lets the story go on", test_starter_rng_failure},
        .expectation = {},
    },
    {
        .input = {"Run Starter RNG: a failed 00.02 ends the program", test_starter_rng_row_is_fatal},
        .expectation = {},
    },
};

void add_tests_AutoStory_StoryTable(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_STORY_TABLE_CASES){
        database.add<Test_AutoStoryStoryTable>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
