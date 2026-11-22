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
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "PokemonBDSP_AutoStory_StarterRng.h"
#include "PokemonBDSP_AutoStory_StoryActions.h"
#include "PokemonBDSP_AutoStory_StoryCheckpoint.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryTable_Tests.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_TableRoutes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::TilePos;


//  The table: unique ids in order, each row has unique non-empty step names
//  and known S11 actions.
UnitTestResult test_story_table_lints(Logger&){
    std::set<std::string> ids;
    std::string previous;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (!ids.insert(row.id).second || row.id <= previous){
            return row.id + ": repeated or out of order.";
        }
        previous = row.id;
        if ((row.on_failure == StoryFailurePolicy::RELOAD_AND_RETRY) == row.saves_in.empty()){
            return row.id + ": a row names the map its save is made in (saves_in) exactly when it retries.";
        }
        if (row.steps.empty()){
            return row.id + ": has no steps.";
        }
        std::set<std::string> step_names;
        for (const BdspStep& step : row.steps){
            if (step.name.empty() || !step_names.insert(step.name).second){
                return std::format("{} {}: empty or duplicate step name.", row.id, step.name);
            }
            if (!is_known_bdsp_action(step.action)){
                return std::format("{} {}: unknown action \"{}\".", row.id, step.name, step.action);
            }
        }
    }
    if (!ids.contains("00.00") || !ids.contains("01.11") || story_checkpoints().front().on_failure != StoryFailurePolicy::STOP){
        return "Expected 00.00 (which stops on a failure) to 01.11.";
    }
    return true;
}

//  Where a row's last step says it leaves the player: the end of its scene,
//  or where its route (or heal) goes. None for a step that does not say.
std::optional<GridPose> end_pose(const StoryCheckpoint& row){
    if (row.steps.empty()){
        return std::nullopt;
    }
    const BdspStep& last = row.steps.back();
    if (last.pose_after){
        return *last.pose_after;
    }
    if (last.action == "route"){
        return std::get<RouteStepArgs>(last.args).target;
    }
    return std::nullopt;
}

//  Each row starts where the row before it ends: every tile its start allows
//  is one the last row's end allows, on the same map. Checked where the last
//  step says where it ends.
UnitTestResult test_story_table_rows_join(Logger&){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    for (size_t i = 1; i < rows.size(); i++){
        const std::optional<GridPose> end = end_pose(rows[i - 1]);
        const std::optional<GridPose>& start = rows[i].start;
        if (!end || !start){
            continue;
        }
        bool joins = start->map == end->map;
        for (const TilePos& pos : start->tiles){
            joins = joins && std::find(end->tiles.begin(), end->tiles.end(), pos) != end->tiles.end();
        }
        if (!joins){
            return std::format("{} starts at {}, but {} ends at {}.",
                rows[i].id, format_grid_pose(*start), rows[i - 1].id, format_grid_pose(*end));
        }
    }
    return true;
}

struct LadyLeg{
    Direction direction;
    size_t tiles;
    TilePos end;
    std::string name;
};

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
    for (int below = 1; below <= 3; below++){
        const std::string error = follow_mart_lady(maps, legs, sandgem({17, 10 + below}), logger);
        if (!error.empty()){
            return std::format("From {} below the door: {}", below, error);
        }
    }
    return true;
}

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

//  plan_story_run() with Starter RNG handoff (E9): stops after 00.01 with
//  handoff_saves_in = 00.02's saves_in ("Verity Lakefront"), ending at
//  VerityLakefront (17, 14) facing north.
UnitTestResult test_plan_story_run_starter_rng(Logger&){
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    const StoryRunPlan full = plan_story_run(table, 0, table.size() - 1, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::OFF});
    if (full.stop != StoryHandoff::NONE || full.rows.size() != table.size() || !full.handoff_saves_in.empty()){
        return "With Starter RNG handoff off, expected the full table with no stop.";
    }
    const StoryRunPlan handoff = plan_story_run(table, 0, table.size() - 1, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::STOP});
    if (handoff.stop != StoryHandoff::STARTER_RNG || handoff.rows.size() != 2 ||
        handoff.rows[0].id != "00.00" || handoff.rows[1].id != "00.01"
    ){
        return "Expected a 00.00..01.11 run with Starter RNG handoff to stop after 00.01.";
    }
    if (handoff.handoff_saves_in != table[2].saves_in || table[2].saves_in.empty()){
        return std::format(
            "Expected the E9 handoff end-save to check the card for \"{}\", got \"{}\".",
            table[2].saves_in, handoff.handoff_saves_in
        );
    }
    const StoryCheckpoint& lakefront_row = handoff.rows.back();
    const std::optional<GridPose> end = end_pose(lakefront_row);
    const std::optional<GridPose>& next_start = table[2].start;
    if (!end || !next_start || end->map != "VerityLakefront" ||
        format_grid_pose(*end) != format_grid_pose(*next_start)
    ){
        return "Expected 00.01 to end at 00.02's VerityLakefront (17, 14) start pose.";
    }
    const StoryRunPlan from_0002 = plan_story_run(table, 2, 4, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::STOP});
    if (from_0002.stop != StoryHandoff::NONE || from_0002.rows.size() != 3 || from_0002.rows.front().id != "00.02"){
        return "Expected a run starting at 00.02 to pass through without stopping for 00.01's handoff.";
    }
    return true;
}

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

struct PlannedStoryRoute{
    std::string name;
    const StoryWorld* world;
    std::vector<TilePos> from;
    std::optional<std::vector<PlannedLeg>> legs;
};

std::vector<PlannedStoryRoute> plan_story_routes(Logger& logger){
    std::vector<PlannedStoryRoute> routes;
    for (const TableRoute& route : table_routes()){
        if (route.world == nullptr){
            logger.log(std::format("{} {}: not checked, from {}.", route.row, route.step->name,
                route.from ? format_grid_pose(*route.from) : "nowhere known"));
            continue;
        }
        RouteStepPlan plan = plan_table_route(route);
        routes.push_back({route.row + " " + route.step->name, route.world, std::move(plan.start), std::move(plan.legs)});
    }
    return routes;
}

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

std::string along_wander_zone(const StoryWorld& world, const TilePos& from, const TileLeg& leg){
    const std::vector<TilePos> tiles = walked_tiles(world, from, leg);
    for (size_t i = 1; i < tiles.size(); i++){
        if (wandering_hazard(world, tiles[i - 1]) && wandering_hazard(world, tiles[i])){
            return std::format("{} from {} runs along a wander zone at {}", to_string(leg), describe(world, from), describe(world, tiles[i]));
        }
    }
    return "";
}

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
    const BdspStep& step = *route.step;
    const RouteStepArgs& args = std::get<RouteStepArgs>(step.args);
    std::string out = std::format("== {} {} (costs: {})\n", route.row, step.name, args.variant.empty() ? "route" : args.variant);
    out += "  from: " + (route.from ? format_grid_pose(*route.from) : std::string("unknown")) + "\n";
    out += "  target: " + format_grid_pose(args.target) + "\n";
    if (route.world == nullptr){
        return out + (route.from && !route.from->tiles.empty()
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

std::string route_plans_golden_path(){
    // code-health: allow char_pointer -- std::getenv returns a const char pointer
    const char* path = std::getenv("PA_ROUTE_PLANS_GOLDEN");
    if (path != nullptr && path[0] != '\0'){
        return path;
    }
    const std::string self = __FILE__;
    const size_t slash = self.find_last_of("/\\");
    return (slash == std::string::npos ? std::string() : self.substr(0, slash + 1)) + "Data/route_plans.golden.txt";
}

std::string plain_lines(const std::string& text){
    std::string out = text.starts_with("\xef\xbb\xbf") ? text.substr(3) : text;
    std::erase(out, '\r');
    return out;
}

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

UnitTestResult test_route_plans_golden(Logger& logger){
    const std::string path = route_plans_golden_path();
    const std::string planned = route_plans_text();
    // code-health: allow char_pointer -- std::getenv returns a const char pointer
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

UnitTestResult test_plan_story_run_in_program(Logger&){
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    const StoryRunPlan in_program = plan_story_run(
        table, 0, table.size() - 1, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::RUN}
    );
    if (in_program.stop != StoryHandoff::NONE || in_program.rows.size() != table.size()){
        return "With Run Starter RNG in AutoStory on, expected the full table with no handoff stop.";
    }
    const StoryCheckpoint& rng_row = in_program.rows[2];
    if (rng_row.id != "00.02" || rng_row.saves_in != "Verity Lakefront" || rng_row.steps.size() != 1 ||
        rng_row.steps[0].action != "starter_rng" || !story_events().contains(rng_row.steps[0].action)
    ){
        return "Expected 00.02 with Run Starter RNG in AutoStory to run the starter_rng event.";
    }
    const std::optional<GridPose> rng_end = end_pose(rng_row);
    const std::optional<GridPose>& row0003_start = table[3].start;
    if (!rng_end || !row0003_start || format_grid_pose(*rng_end) != format_grid_pose(*row0003_start)){
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
        .input = {"01.08: the walk below the Ravaged Path keeps off the youngster's row (lab-B, 2026-11-08)", test_ravaged_path_keeps_off_the_youngster},
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
