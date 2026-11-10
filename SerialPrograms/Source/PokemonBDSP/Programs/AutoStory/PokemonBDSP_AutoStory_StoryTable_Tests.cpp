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
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_TableRoutes.h"
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

//  The variants each kind does itself, besides the story's events.
const std::map<StoryStepKind, std::set<std::string>>& built_in_variants(){
    static const std::map<StoryStepKind, std::set<std::string>> variants{
        {StoryStepKind::ROUTE, {"", "town", "robust", "line"}},
        {StoryStepKind::LEGS, {"", "one_short_ok", "to_wall", "counted_wall", "measured", "to_map"}},
        {StoryStepKind::STEP, {"", "to_banner", "tap", "or_legs"}},
        {StoryStepKind::MASH, {"", "a"}},
        {StoryStepKind::BATTLE, {"", "doubles", "starter"}},
    };
    return variants;
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
            const auto built_in = built_in_variants().find(step.kind);
            const bool known = events.contains(step.variant) ||
                (step.kind != StoryStepKind::GAME && step.variant.empty()) ||
                (built_in != built_in_variants().end() && built_in->second.contains(step.variant));
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

//  Where a row's last step says it leaves the player: the end of its scene,
//  or where its route (or heal) goes. None for a step that does not say.
std::optional<Pokemon::Pose> end_pose(const StoryCheckpoint& row){
    if (row.steps.empty()){
        return std::nullopt;
    }
    const Pokemon::StoryStep& last = row.steps.back();
    return last.pose_after ? last.pose_after : last.target;
}

//  Each row starts where the row before it ends: every tile its start allows
//  is one the last row's end allows, on the same map. Checked where the last
//  step says where it ends.
UnitTestResult test_story_table_rows_join(Logger&){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    for (size_t i = 1; i < rows.size(); i++){
        const std::optional<Pokemon::Pose> end = end_pose(rows[i - 1]);
        const std::optional<Pokemon::Pose>& start = rows[i].start;
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

//  01.08's walk below the Ravaged Path keeps off the row Route 204 South's
//  youngster_3 wanders, (12, 2) to (14, 2): on lab-B (2026-11-08) the plan
//  "north 35 (to wall) to (13, 2), west 2 (to wall) to (11, 2)" walked along
//  it, and he stopped the leg west twice. A leg may cross his row through
//  one of his tiles, not walk two of them.
UnitTestResult test_ravaged_path_keeps_off_the_youngster(Logger& logger){
    for (const PlannedStoryRoute& route : plan_story_routes(logger)){
        if (route.name != "01.08 below_the_ravaged_path"){
            continue;
        }
        if (!route.legs){
            return "01.08 below_the_ravaged_path: no plan.";
        }
        const StoryWorld& world = *route.world;
        for (const PlannedLeg& planned : *route.legs){
            logger.log("01.08 below_the_ravaged_path: " + to_string(planned));
        }
        const auto youngster_row = [&](const TilePos& pos){
            const std::optional<TilePos> local = story_to_local(world, "Route204South", pos);
            return local && local->y == 2 && local->x >= 12 && local->x <= 14;
        };
        const auto [most, leg] = most_zone_tiles(world, route.from, *route.legs, youngster_row);
        if (most >= 2){
            return std::format("the leg {} walks {} tiles of the youngster's row (12..14, 2).", leg, most);
        }
        return true;
    }
    return "01.08 below_the_ravaged_path is not a ROUTE step from a known place.";
}


//  The route plan dump: every table ROUTE step's legs, in a golden file
//  beside the story data, so a planner or table change shows its plans' diff
//  in review.

constexpr const char* ROUTE_PLANS_HEADER =
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
    std::string out = std::format("== {} {} (costs: {})\n", route.row, step.name, step.variant.empty() ? "route" : step.variant);
    out += "  from: " + (route.from ? Pokemon::to_string(*route.from) : std::string("unknown")) + "\n";
    out += "  target: " + Pokemon::to_string(*step.target) + "\n";
    if (route.world == nullptr){
        return out + (route.from && !Pokemon::tiles_of(*route.from).empty()
            ? "  not planned: no story world joins the two maps\n"
            : "  not planned: the start is unknown (a LEGS or STEP step before it does not say where it ends)\n");
    }
    return out + dump_plan(*route.world, plan_table_route(route));
}

std::string route_plans_text(){
    std::string out = ROUTE_PLANS_HEADER;
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
        .input = {"01.08: the walk below the Ravaged Path keeps off the youngster's row (lab-B, 2026-11-08)", test_ravaged_path_keeps_off_the_youngster},
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
