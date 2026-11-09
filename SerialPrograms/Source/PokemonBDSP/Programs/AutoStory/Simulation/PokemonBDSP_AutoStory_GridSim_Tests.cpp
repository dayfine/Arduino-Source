/*  BDSP AutoStory Grid Simulation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The grid simulation's moves against the console's measurements, then the
 *  route walker (walk_route()) on it: every route of the story table from
 *  where the table has the player, and the routes to doors that a turn on
 *  the stick missed on the console (r49, r57).
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_OldStorySteps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_GridSim_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::Pose;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TileBox;
using Pokemon::TilePos;


//  A map of `rows`, with someone wandering `wander` if given.
StoryWorld make_world(const std::vector<std::string>& rows, const std::optional<TileBox>& wander = std::nullopt){
    Pokemon::MapData map;
    map.name = "Test";
    map.tiles = Pokemon::Grid<Pokemon::TileKind>{(int)rows[0].size(), (int)rows.size(), {}};
    for (const std::string& row : rows){
        for (char glyph : row){
            map.tiles.cells.push_back(*Pokemon::tile_from_glyph(glyph));
        }
    }
    if (wander){
        map.objects.push_back({.name = "wanderer", .x = wander->x0, .y = wander->y0, .facing = std::nullopt,
            .wander = wander, .trainer = std::nullopt, .hide_flag = std::nullopt});
    }
    StoryWorld world;
    world.add("Test", map, {0, 0});
    return world;
}

SimPos at_tile(int x, int y, int part_x = 0, int part_y = 0){
    return {x * SIM_TILE + part_x, y * SIM_TILE + part_y};
}


//  One input to the simulation.
enum class SimInputKind{
    DPAD,
    STICK,
    HOLD,
};
struct SimInput{
    SimInputKind kind;
    Direction direction;
    Milliseconds time;
};

//  Inputs from `start`, and where the player must end and how the last
//  input ended.
struct MoveCase{
    std::string name;
    TilePos start;
    std::vector<SimInput> inputs;
    SimPos end;
    SimStop stop;
};

SimMove apply(GridSim& sim, const SimInput& input){
    switch (input.kind){
    case SimInputKind::DPAD:
        return sim.tap(input.direction, input.time, false);
    case SimInputKind::STICK:
        return sim.tap(input.direction, input.time, true);
    case SimInputKind::HOLD:
        return sim.hold(input.direction, input.time, SimPace::WALK);
    }
    return {};
}

//  A room with a door in row 3 at x 3, a ledge south at x 7, someone
//  standing at (5, 1), and a trainer east of row 2 who looks west 3 tiles.
UnitTestResult test_moves(Logger&){
    StoryWorld world = make_world({
        "#########",
        "#.......#",
        "#.......#",
        "###D###v#",
        "#.......#",
        "#########",
    });
    world.block({5, 1});
    const SimWorld setup{.world = &world, .trainers = {{.name = "watcher", .pos = {8, 2}, .facing = Direction::WEST, .range = 3}}};
    constexpr Direction EAST = Direction::EAST;
    constexpr Direction SOUTH = Direction::SOUTH;
    constexpr Direction WEST = Direction::WEST;
    const std::vector<MoveCase> cases{
        {.name = "a 50 ms D-pad tap only turns", .start = {2, 1}, .inputs = {{SimInputKind::DPAD, EAST, 50ms}}, .end = at_tile(2, 1), .stop = SimStop::DONE},
        {.name = "a 160 ms D-pad tap steps a tile", .start = {2, 1}, .inputs = {{SimInputKind::DPAD, EAST, 160ms}}, .end = at_tile(3, 1), .stop = SimStop::DONE},
        {.name = "a 100 ms stick tap steps 0.6 tile across", .start = {2, 1}, .inputs = {{SimInputKind::STICK, EAST, 100ms}}, .end = at_tile(2, 1, 625), .stop = SimStop::DONE},
        {.name = "a 100 ms stick tap steps 0.4 tile down", .start = {2, 1}, .inputs = {{SimInputKind::STICK, SOUTH, 100ms}}, .end = at_tile(2, 1, 0, 416), .stop = SimStop::DONE},
        {.name = "the D-pad keeps a stick tap's offset", .start = {2, 1},
            .inputs = {{SimInputKind::STICK, EAST, 100ms}, {SimInputKind::DPAD, EAST, 160ms}}, .end = at_tile(3, 1, 625), .stop = SimStop::DONE},
        {.name = "a held walk stops at someone standing", .start = {2, 1}, .inputs = {{SimInputKind::HOLD, EAST, 5000ms}}, .end = at_tile(4, 1), .stop = SimStop::BLOCKED},
        {.name = "a door warps from its column", .start = {3, 2}, .inputs = {{SimInputKind::HOLD, SOUTH, 3000ms}}, .end = at_tile(3, 3), .stop = SimStop::WARP},
        {.name = "part of a tile off the door's column, the frame stops us", .start = {3, 2},
            .inputs = {{SimInputKind::STICK, WEST, 100ms}, {SimInputKind::HOLD, SOUTH, 3000ms}}, .end = at_tile(2, 2, 375), .stop = SimStop::BLOCKED},
        {.name = "a trainer's sight stops a walk", .start = {1, 2}, .inputs = {{SimInputKind::HOLD, EAST, 5000ms}}, .end = at_tile(5, 2), .stop = SimStop::DIALOG},
        {.name = "a ledge hops two tiles", .start = {7, 2}, .inputs = {{SimInputKind::DPAD, SOUTH, 160ms}}, .end = at_tile(7, 4), .stop = SimStop::DONE},
    };
    for (const MoveCase& item : cases){
        GridSim sim(setup, item.start, Direction::NORTH, false);
        SimMove last;
        for (const SimInput& input : item.inputs){
            last = apply(sim, input);
        }
        if (sim.player().pos != item.end || last.stop != item.stop){
            return std::format("{}: expected {} ({}), got {} ({}).", item.name, to_string(item.end), sim_stop_name(item.stop),
                to_string(sim.player().pos), sim_stop_name(last.stop));
        }
    }
    return true;
}


//  A turn on the stick, as before r49: each turn tap of the D-pad becomes a
//  100 ms stick tap, which moves the player part of a tile.
class StickTurnIO : public RouteIO{
public:
    explicit StickTurnIO(RouteIO& io)
        : m_io(io)
    {}
    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override{
        return m_io.walk_to_wall(walk);
    }
    virtual WalkResult walk_taps(Direction direction, size_t tiles, const std::string& label) override{
        return m_io.walk_taps(direction, tiles, label);
    }
    virtual void press(const RouteInput& input) override{
        if (input.kind != RouteInputKind::DPAD || input.hold >= 100ms){
            m_io.press(input);
            return;
        }
        RouteInput stick = input;
        stick.kind = RouteInputKind::STICK;
        stick.hold = 100ms;
        m_io.press(stick);
    }
    virtual bool dialog_within(Milliseconds wait, Color color) override{
        return m_io.dialog_within(wait, color);
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


//  A route walked on the simulation: where the walker believes it ended,
//  where the player is, and why it failed.
struct SimWalk{
    std::vector<TilePos> believed;
    SimPos actual;
    std::string failure;
    std::vector<std::string> notes;
};

//  One route to walk: on `world`, from `from` (the walker's belief) with the
//  player on `start`, to any of `targets`.
struct SimRoute{
    std::string label;
    const StoryWorld* world;
    std::vector<TilePos> from;
    TilePos start;
    std::vector<TilePos> targets;
    RobustPlanCosts costs;
    bool can_run;
};

SimWalk walk_on_sim(const SimRoute& route, bool stick_turns){
    SimWalk out;
    GridSim sim({.world = route.world, .trainers = story_trainers(*route.world)}, route.start, Direction::NORTH, route.can_run);
    GridSimRouteIO sim_io(sim, out.notes);
    StickTurnIO stick_io(sim_io);
    RouteIO& io = stick_turns ? static_cast<RouteIO&>(stick_io) : sim_io;
    RouteTrainers trainers{story_trainers(*route.world), [&sim](const std::string&){ sim.end_dialogue(); }};
    const std::vector<TilePos>& targets = route.targets;
    std::vector<TilePos> avoid = wandering_hazards(*route.world);
    std::erase_if(avoid, [&](const TilePos& pos){
        return std::find(route.from.begin(), route.from.end(), pos) != route.from.end()
            || std::find(targets.begin(), targets.end(), pos) != targets.end();
    });
    try{
        out.believed = walk_route(io, *route.world, route.from, trainers, {
            .goal = [&targets](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); },
            .label = route.label,
            .avoid = std::move(avoid),
            .costs = route.costs,
            .speed = route.can_run ? MoveSpeed::RUN : MoveSpeed::WALK,
        });
    }catch (const GridSimFailure& e){
        out.failure = e.message;
    }
    out.actual = sim.player().pos;
    return out;
}

//  Why a walk did not end where it should: on the grid, on one of the
//  targets, and on a tile the walker believes. Empty if it did.
std::string walk_error(const SimRoute& route, const SimWalk& walk){
    const auto has = [](const std::vector<TilePos>& tiles, const TilePos& pos){
        return std::find(tiles.begin(), tiles.end(), pos) != tiles.end();
    };
    const TilePos tile = nearest_tile(walk.actual);
    if (!walk.failure.empty()){
        return std::format("{} from {}: failed at {}: {}", route.label, describe(*route.world, route.start),
            to_string(walk.actual), walk.failure);
    }
    if (!on_grid(walk.actual) || !has(route.targets, tile) || !has(walk.believed, tile)){
        return std::format("{} from {}: ended at {} ({}), off the grid or off the target.", route.label,
            describe(*route.world, route.start), to_string(walk.actual), describe(*route.world, tile));
    }
    return "";
}


std::vector<TilePos> world_tiles(const StoryWorld& world, const Pose& pose){
    std::vector<TilePos> tiles;
    for (const TilePos& pos : Pokemon::tiles_of(pose)){
        tiles.push_back(story_to_world(world, pose.map, pos));
    }
    return tiles;
}

//  Where the table has the player after `step`, from `at`: a route's
//  target, a walk's last tile, a step's next tile; a scene or an event's
//  end is the table's pose_after, or unknown.
std::optional<Pose> pose_after_step(const std::optional<Pose>& at, const StoryStep& step){
    std::optional<Pose> next;
    if (step.kind == StoryStepKind::ROUTE){
        next = step.target;
    }else if (at && step.kind == StoryStepKind::STEP){
        std::vector<TilePos> tiles;
        for (const TilePos& pos : Pokemon::tiles_of(*at)){
            tiles.push_back(pos.next(step.direction));
        }
        next = Pokemon::tile_pose(at->map, tiles, step.direction);
    }else if (at && step.kind == StoryStepKind::LEGS && step.variant.empty()){
        std::vector<TilePos> tiles = Pokemon::tiles_of(*at);
        for (const Pokemon::StoryLeg& leg : step.legs){
            for (TilePos& pos : tiles){
                for (int i = 0; i < leg.tiles; i++){
                    pos = pos.next(leg.direction);
                }
            }
        }
        next = Pokemon::tile_pose(at->map, tiles, step.legs.back().direction);
    }else if (step.kind != StoryStepKind::GAME && step.kind != StoryStepKind::SHOP &&
        step.kind != StoryStepKind::MASH && step.kind != StoryStepKind::LEGS
    ){
        next = at;
    }
    return step.pose_after ? step.pose_after : next;
}

//  Every ROUTE step of the table that starts from a known pose, from its
//  first and its last tile, at the pace StoryExecutor walks it.
std::vector<SimRoute> table_routes(){
    std::vector<SimRoute> routes;
    for (const StoryCheckpoint& row : story_checkpoints()){
        std::optional<Pose> at = row.start;
        bool shoes = row.id > std::string(RUNNING_SHOES_ROW);
        for (const StoryStep& step : row.steps){
            const StoryWorld* world = at && step.kind == StoryStepKind::ROUTE ? story_plan_world(at->map, step.target->map) : nullptr;
            const std::vector<TilePos> from = world ? world_tiles(*world, *at) : std::vector<TilePos>{};
            std::vector<TilePos> starts;
            if (!from.empty()){
                starts.push_back(from.front());
            }
            if (from.size() > 1){
                starts.push_back(from.back());
            }
            for (const TilePos& start : starts){
                routes.push_back({.label = row.id + " " + step.name, .world = world, .from = from, .start = start,
                    .targets = world_tiles(*world, *step.target), .costs = *story_route_costs(step.variant), .can_run = shoes});
            }
            at = pose_after_step(at, step);
            shoes = shoes || (row.id == RUNNING_SHOES_ROW && step.name == RUNNING_SHOES_STEP);
        }
    }
    return routes;
}

UnitTestResult test_table_routes(Logger& logger){
    std::string errors;
    size_t walked = 0;
    for (const SimRoute& route : table_routes()){
        const SimWalk walk = walk_on_sim(route, false);
        walked++;
        const std::string error = walk_error(route, walk);
        logger.log(std::format("{} from {}: {}", route.label, describe(*route.world, route.start), error.empty() ? "on target" : error));
        if (!error.empty()){
            errors += "\n    " + error;
        }
    }
    if (walked < 10){
        return std::format("Expected the table's routes; found {}.", walked);
    }
    return errors.empty() ? UnitTestResult(true) : UnitTestResult("Routes that did not end on their target:" + errors);
}


//  A route to the tile below a door, then a walk north into it.
struct DoorRoute{
    std::string name;
    std::string map;
    TilePos from;
    std::string door;
};

//  Whether the route ends below the door and a walk north goes in.
std::string door_route_error(const DoorRoute& item, bool stick_turns){
    const StoryWorld& world = *story_plan_world(item.map, item.map);
    const TilePos from = story_to_world(world, item.map, item.from);
    const TilePos below = story_to_world(world, item.map, map_point(item.map, item.door).next(Direction::SOUTH));
    const SimRoute route{.label = item.name, .world = &world, .from = {from}, .start = from, .targets = {below},
        .costs = town_plan_costs(), .can_run = true};
    const SimWalk walk = walk_on_sim(route, stick_turns);
    const std::string error = walk_error(route, walk);
    if (!error.empty()){
        return error;
    }
    GridSim sim({.world = &world}, below, Direction::NORTH, true);
    if (sim.hold(Direction::NORTH, 3000ms, SimPace::RUN).stop != SimStop::WARP){
        return item.name + ": the walk north did not go through the door.";
    }
    return "";
}

//  r49 put us a tile west of Sandgem's Center door after a stick "turn",
//  and r57 walked the Jubilife School's row a column short: a turn tap on
//  the stick moves part of a tile, and every D-pad tap after keeps it. With
//  D-pad turns both routes end below their doors; with stick turns they
//  must miss, or the simulation would not have caught it.
UnitTestResult test_door_routes(Logger& logger){
    const std::vector<DoorRoute> routes{
        {.name = "Sandgem: below the Center's door", .map = "SandgemTown", .from = map_point("SandgemTown", "front_row_east_end"),
            .door = "pokemon_center_door"},
        {.name = "Jubilife: below the School's door", .map = "JubilifeCity", .from = map_point("JubilifeCity", "pokemon_center_door").next(Direction::SOUTH),
            .door = "trainers_school_door"},
    };
    for (const DoorRoute& item : routes){
        const std::string dpad = door_route_error(item, false);
        if (!dpad.empty()){
            return "D-pad turns: " + dpad;
        }
        const std::string stick = door_route_error(item, true);
        logger.log(item.name + ", stick turns: " + stick);
        if (stick.empty()){
            return item.name + ": with turns on the stick, expected the route to miss the door; it went in.";
        }
    }
    return true;
}


//  The story's walking steps on a GridSim: the route walker's IO and walks
//  until something stops us (as the story simulation walks them). Anything
//  else fails the walk.
class WalkStepsIO : public StoryStepIO{
public:
    WalkStepsIO(GridSim& sim, GridSimRouteIO& route)
        : m_sim(sim)
        , m_route(route)
    {}

    virtual RouteIO& route() override{
        return m_route;
    }
    virtual void fight(const std::string& trainer) override{
        unsupported("a fight with " + trainer);
    }
    virtual void walk_until_stopped(Direction direction, size_t tiles, const std::string& label) override{
        m_sim.tap(direction, ROUTE_TURN_TAP, false);
        const Milliseconds limit = ROUTE_START_DELAY + 800ms + route_ms_per_tile(direction) * (int64_t)(2 * tiles);
        const SimMove move = m_sim.hold(direction, limit, SimPace::WALK);
        if (move.stop == SimStop::DIALOG || move.stop == SimStop::WARP){
            m_route.note(label + ": " + sim_stop_name(move.stop) + " instead of a stop.", RouteNote::FAILURE);
        }
    }
    virtual bool run_until_map(Direction, Milliseconds, const std::string& map, const std::string&) override{
        unsupported("a run onto " + map);
        return false;
    }
    virtual bool run_into_door(Direction, bool, Milliseconds, Milliseconds) override{
        unsupported("a run into a door");
        return false;
    }
    virtual std::optional<size_t> tap_to_banner(Direction, const std::string& map, size_t) override{
        unsupported("taps to " + map + "'s banner");
        return std::nullopt;
    }
    virtual void settle(Button, const std::string& label, bool) override{
        unsupported(label + "'s settle");
    }

private:
    void unsupported(const std::string& what){
        m_route.note(what + " is not simulated here.", RouteNote::FAILURE);
    }

    GridSim& m_sim;
    GridSimRouteIO& m_route;
};

//  Row `row`'s steps from `first` up to the one before `last`, and where
//  the table has the player before `first`.
struct TableSteps{
    Pose start;
    std::vector<StoryStep> steps;
};
std::optional<TableSteps> table_steps(const std::string& row, const std::string& first, const std::string& last){
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    const auto checkpoint = std::find_if(table.begin(), table.end(), [&row](const StoryCheckpoint& item){ return item.id == row; });
    if (checkpoint == table.end()){
        return std::nullopt;
    }
    std::optional<TableSteps> out;
    std::optional<Pose> at = checkpoint->start;
    for (const StoryStep& step : checkpoint->steps){
        if (step.name == last){
            return out;
        }
        if (step.name == first && at){
            out = TableSteps{*at, {}};
        }
        if (out){
            out->steps.push_back(step);
        }
        at = pose_after_step(at, step);
    }
    return std::nullopt;
}

//  Where each of `table`'s steps, walked on `sim` from wherever its player
//  stands, left the player, and why the walk failed if it did.
struct StepsWalk{
    std::vector<std::pair<std::string, SimPos>> ends;
    std::string failure;
};
StepsWalk walk_steps(GridSim& sim, const TableSteps& table){
    StepsWalk out;
    std::vector<std::string> notes;
    GridSimRouteIO route_io(sim, notes);
    WalkStepsIO io(sim, route_io);
    StoryWalkSteps walk(io);
    walk.begin_attempt("01.03");
    Pokemon::RunState state{.pose = table.start, .flags = {}};
    try{
        for (const StoryStep& step : table.steps){
            const Pokemon::Outcome outcome = step.kind == StoryStepKind::ROUTE ? walk.route(state, step)
                : step.kind == StoryStepKind::LEGS ? walk.legs(state, step)
                : Pokemon::outcome_failed("not_a_walk", step.name);
            out.ends.emplace_back(step.name, sim.player().pos);
            if (!outcome.succeeded()){
                out.failure = step.name + ": " + Pokemon::to_string(outcome);
                break;
            }
            state.pose = step.pose_after ? *step.pose_after : outcome.pose ? *outcome.pose : state.pose;
        }
    }catch (const GridSimFailure& e){
        out.failure = e.message;
    }
    return out;
}
//  Where `table`'s steps, walked from `start` instead of where the table
//  has the player, failed or left the player off `checks`' tiles; empty if
//  they did not.
std::string walk_misses(const StoryWorld& world, const TableSteps& table, const TilePos& start,
    const std::vector<std::pair<std::string, TilePos>>& checks, Logger& logger){
    GridSim sim({.world = &world, .trainers = story_trainers(world)}, start, Direction::SOUTH, true);
    const StepsWalk walk = walk_steps(sim, table);
    std::string ends;
    for (const auto& [name, end] : walk.ends){
        ends += std::format(" {} {}", name, to_string(end));
    }
    logger.log(std::format("from {}:{}{}", Pokemon::to_string(start), ends, walk.failure.empty() ? "" : ", failed: " + walk.failure));
    if (!walk.failure.empty()){
        return std::format("from {}: failed at {}: {}", Pokemon::to_string(start), to_string(sim.player().pos), walk.failure);
    }
    for (const auto& [name, tile] : checks){
        const auto end = std::find_if(walk.ends.begin(), walk.ends.end(), [&name](const auto& item){ return item.first == name; });
        if (end == walk.ends.end() || end->second != sim_pos(tile)){
            return std::format("from {}: {} ended{}, not on {}.", Pokemon::to_string(start), name,
                end == walk.ends.end() ? " nowhere" : " at " + to_string(end->second), Pokemon::to_string(tile));
        }
    }
    return {};
}

//  Roark's room the old way: counted taps across the room, down column 29
//  and onto his row from (15, 16), then west against the rock east of him
//  and south against the wall.
TableSteps old_taps_across_roarks_room(){
    StoryStep taps;
    taps.kind = StoryStepKind::LEGS;
    taps.name = "above_roarks_row";
    taps.legs = {
        {.direction = Direction::EAST, .tiles = 14}, {.direction = Direction::SOUTH, .tiles = 10},
        {.direction = Direction::WEST, .tiles = 2}, {.direction = Direction::SOUTH, .tiles = 2},
    };
    StoryStep walls;
    walls.kind = StoryStepKind::LEGS;
    walls.name = "beside_roarks_rock";
    walls.variant = "to_wall";
    walls.legs = {{.direction = Direction::WEST, .tiles = 7}, {.direction = Direction::SOUTH, .tiles = 1}};
    return {Pokemon::tile_pose("OreburghMine", {TilePos{15, 16}}, Direction::SOUTH), {taps, walls}};
}

//  lab-B 2026-11-08 11:55: a wild battle on 01.03's counted taps through
//  Roark's room re-planned to the end of the interrupted leg, (15, 16), and
//  the re-plan's open run south overshot a row; the taps after landed a row
//  too far south, the walk west went under Roark's rocks, and no one
//  answered the talk. From a row south of (15, 16), the old taps miss
//  beside his rock (20, 29): the sim shows the console's failure. The
//  wild battle itself: Recovery walk tests, "01.03 to Roark's corner".
UnitTestResult test_roarks_room_from_a_row_off(Logger& logger){
    const StoryWorld& world = *story_plan_world("OreburghMine", "OreburghMine");
    const std::string miss = walk_misses(world, old_taps_across_roarks_room(), {15, 17}, {{"beside_roarks_rock", {20, 29}}}, logger);
    if (miss.empty()){
        return "The old taps across Roark's room reached (20, 29) from a row south: the test no longer shows the console's failure.";
    }
    return true;
}

//  lab-B 2026-11-08 15:31 to 15:54: 01.05's open walk west 3 from (5, 13),
//  where Barry's push leaves us, to (2, 13) beside the Oreburgh Gate's mouth
//  (1, 13) went on into the Gate. D-pad taps move whole tiles but keep a
//  part-tile offset: 0.375 of a tile west of (5, 13), the three taps take
//  the warp; on the tile, they stop beside it.
UnitTestResult test_gate_walk_part_tile_off(Logger&){
    const StoryWorld& world = *story_plan_world("OreburghCity", "OreburghCity");
    for (const bool off : {false, true}){
        GridSim sim({.world = &world}, story_to_world(world, "OreburghCity", {5, 13}), Direction::WEST, true);
        if (off){
            sim.tap(Direction::WEST, 60ms, true);
        }
        SimStop stop = SimStop::DONE;
        for (int tap = 0; tap < 3 && stop == SimStop::DONE; tap++){
            stop = sim.tap(Direction::WEST, 160ms, false).stop;
        }
        if ((stop == SimStop::WARP) != off){
            return std::format("{} (5, 13): three taps west ended {} at {}.", off ? "0.375 west of" : "on",
                sim_stop_name(stop), to_string(sim.player().pos));
        }
    }
    return true;
}

//  lab-B 2026-11-08 18:41: 01.11's 8 taps west, then north, ended below the
//  old man, and no one answered the old woman's talk. Floaroma's banner
//  stops us on any of (10..12, 27..29): the old taps miss her from x 10 and
//  11; the table's walk west to the field's edge and 4 taps east end below
//  her from all nine.
UnitTestResult test_old_couple_from_each_arrival(Logger& logger){
    const std::optional<TableSteps> table = table_steps("01.11", "west_to_the_field_edge", "old_woman");
    const StoryWorld* world = story_plan_world("FloaromaTown", "FloaromaTown");
    if (!table || table->steps.empty() || world == nullptr){
        return "01.11 has no walk west to the field's edge, or Floaroma no map.";
    }
    const TableSteps old_taps{table->start, {old_west_along_the_field(), table->steps.back()}};
    const TilePos below_her = story_to_world(*world, "FloaromaTown", map_point("FloaromaTown", "old_woman").next(Direction::SOUTH));
    for (int x = 10; x <= 12; x++){
        for (int y = 27; y <= 29; y++){
            const TilePos start = story_to_world(*world, "FloaromaTown", {x, y});
            const std::string old_miss = walk_misses(*world, old_taps, start, {{"up_to_the_old_woman", below_her}}, logger);
            if (old_miss.empty() != (x == 12)){
                return std::format("The old taps from ({}, {}): {}", x, y, old_miss.empty() ? "ended below her." : old_miss);
            }
            const std::string miss = walk_misses(*world, *table, start, {{"up_to_the_old_woman", below_her}}, logger);
            if (!miss.empty()){
                return "The table's walk " + miss;
            }
        }
    }
    return true;
}

UnitTestResult test_wandering_npc_mechanics(Logger& logger){
    StoryWorld world = make_world({
        "#######",
        "#.....#",
        "#.....#",
        "#.....#",
        "#######",
    });
    SimWanderer wanderer{
        .name = "wanderer",
        .box = {1, 1, 3, 3},
        .pos = {2, 2},
        .seed = 42,
        .step_interval = 2000ms,
    };
    GridSim sim({.world = &world, .wanderers = {wanderer}}, {1, 2}, Direction::EAST, false);
    sim_advance_time(sim, 100000ms);
    const SimWanderer& after = sim_wanderers(sim).front();
    if (after.pos.x < 1 || after.pos.x > 3 || after.pos.y < 1 || after.pos.y > 3){
        return "wanderer stepped outside its bounding box";
    }
    GridSim sim2({.world = &world, .wanderers = {wanderer}}, {1, 2}, Direction::EAST, false);
    sim_advance_time(sim2, 100000ms);
    if (sim_wanderers(sim2).front().pos != after.pos){
        return "wanderer simulation is not deterministic across identical seeds";
    }
    return true;
}

UnitTestResult test_wandering_npc_collision_and_dialog(Logger& logger){
    StoryWorld world = make_world({
        "#####",
        "#...#",
        "#####",
    });
    SimWanderer wanderer{
        .name = "blocker",
        .box = {2, 1, 2, 1},
        .pos = {2, 1},
        .seed = 0,
        .step_interval = 10000ms,
    };
    GridSim sim({.world = &world, .wanderers = {wanderer}}, {1, 1}, Direction::EAST, false);
    const SimMove move = sim.tap(Direction::EAST, 160ms, false);
    if (move.stop != SimStop::BLOCKED || sim.player().pos != sim_pos({1, 1})){
        return "player was not blocked by wandering NPC";
    }
    if (!sim.press_a()){
        return "pressing A facing wandering NPC failed to open dialogue";
    }
    return true;
}

UnitTestResult test_wandering_npc_routes_multi_seed(Logger& logger){
    const StoryWorld* world = story_plan_world("OreburghCity", "OreburghCity");
    if (world == nullptr){
        return "OreburghCity world not found";
    }
    const TilePos start = story_to_world(*world, "OreburghCity", {26, 22});
    const TilePos target = story_to_world(*world, "OreburghCity", {46, 58});
    const TileBox hiker_box{
        story_to_world(*world, "OreburghCity", {46, 25}).x,
        story_to_world(*world, "OreburghCity", {46, 25}).y,
        story_to_world(*world, "OreburghCity", {48, 27}).x,
        story_to_world(*world, "OreburghCity", {48, 27}).y
    };
    const TilePos hiker_home = story_to_world(*world, "OreburghCity", {47, 26});
    const std::vector<uint64_t> seeds = {0, 1, 2, 7, 42, 100, 999, 1337};
    const std::vector<Milliseconds> start_times = {0ms, 500ms, 1200ms, 2000ms, 3500ms, 5000ms};
    for (uint64_t seed : seeds){
        for (Milliseconds start_time : start_times){
            SimWanderer hiker{
                .name = "hiker_0",
                .box = hiker_box,
                .pos = hiker_home,
                .seed = seed,
                .step_interval = 2000ms,
                .time_offset = start_time,
            };
            GridSim sim({.world = world, .trainers = story_trainers(*world), .wanderers = {hiker}}, start, Direction::NORTH, true);
            std::vector<std::string> notes;
            GridSimRouteIO sim_io(sim, notes);
            RouteTrainers trainers{story_trainers(*world), [&sim](const std::string&){ sim.end_dialogue(); }};
            const auto result = walk_route(sim_io, *world, {start}, trainers, {
                .goal = [&](const TilePos& pos){ return pos == target; },
                .label = "01.03 above_the_mine",
                .avoid = {},
                .costs = *story_route_costs("town"),
                .speed = MoveSpeed::RUN,
            });
            if (result.empty() || result.front() != target || nearest_tile(sim.player().pos) != target){
                return std::format("route failed for seed {} and start_time {} ms", seed, start_time.count());
            }
        }
    }
    return true;
}

//  Routes never run along a wander zone, but cross one: a row someone
//  wanders, (2..4, 2), crossed south at (3, 2) while they stand there.
UnitTestResult test_wandering_npc_wait_clearance(Logger& logger){
    const TileBox row_zone{.x0 = 2, .y0 = 2, .x1 = 4, .y1 = 2};
    const StoryWorld room = make_world({"#######", "#.....#", "#.....#", "#.....#", "#.....#", "#######"}, row_zone);
    const StoryWorld* world = &room;
    const TilePos start{3, 1};
    const TilePos target{3, 4};
    SimWanderer hiker{
        .name = "hiker_0",
        .box = row_zone,
        .pos = {3, 2},
        .seed = 2,
        .step_interval = 2000ms,
    };
    GridSim sim({.world = world, .trainers = {}, .wanderers = {hiker}}, start, Direction::NORTH, true);
    std::vector<std::string> notes;
    GridSimRouteIO sim_io(sim, notes);
    RouteTrainers trainers{{}, [&sim](const std::string&){ sim.end_dialogue(); }};
    const auto result = walk_route(sim_io, *world, {start}, trainers, {
        .goal = [&](const TilePos& pos){ return pos == target; },
        .label = "walk_through_hiker",
        .avoid = {},
        .costs = *story_route_costs("town"),
        .speed = MoveSpeed::RUN,
    });
    if (result.empty() || result.front() != target){
        return "wait clearance route did not reach target";
    }
    bool saw_blocked_note = false;
    for (const std::string& n : notes){
        if (n.find("waiting for the way to clear") != std::string::npos){
            saw_blocked_note = true;
            break;
        }
    }
    if (!saw_blocked_note){
        return "expected route to encounter blocked wanderer and wait for clearance";
    }
    return true;
}

UnitTestResult test_wandering_npc_oreburgh_north_multi_seed(Logger& logger){
    const StoryWorld* world = story_plan_world("OreburghCity", "OreburghCity");
    if (world == nullptr){
        return "OreburghCity world not found";
    }
    const TilePos start = story_to_world(*world, "OreburghCity", {46, 52});
    const TilePos target = story_to_world(*world, "OreburghCity", {46, 21});
    const TileBox hiker_box{
        story_to_world(*world, "OreburghCity", {46, 25}).x,
        story_to_world(*world, "OreburghCity", {46, 25}).y,
        story_to_world(*world, "OreburghCity", {48, 27}).x,
        story_to_world(*world, "OreburghCity", {48, 27}).y
    };
    const TilePos hiker_home = story_to_world(*world, "OreburghCity", {47, 26});
    const std::vector<uint64_t> seeds = {0, 1, 2, 7, 42, 100, 999, 1337};
    const std::vector<Milliseconds> start_times = {0ms, 500ms, 1200ms, 2000ms, 3500ms, 5000ms};
    for (uint64_t seed : seeds){
        for (Milliseconds start_time : start_times){
            SimWanderer hiker{
                .name = "hiker_0",
                .box = hiker_box,
                .pos = hiker_home,
                .seed = seed,
                .step_interval = 2000ms,
                .time_offset = start_time,
            };
            GridSim sim({.world = world, .trainers = story_trainers(*world), .wanderers = {hiker}}, start, Direction::NORTH, true);
            std::vector<std::string> notes;
            GridSimRouteIO sim_io(sim, notes);
            RouteTrainers trainers{story_trainers(*world), [&sim](const std::string&){ sim.end_dialogue(); }};
            const auto result = walk_route(sim_io, *world, {start}, trainers, {
                .goal = [&](const TilePos& pos){ return pos == target; },
                .label = "01.03 oreburgh_north",
                .avoid = {},
                .costs = *story_route_costs("town"),
                .speed = MoveSpeed::RUN,
            });
            if (result.empty() || result.front() != target || nearest_tile(sim.player().pos) != target){
                return std::format("oreburgh north route failed for seed {} and start_time {} ms", seed, start_time.count());
            }
        }
    }
    return true;
}

class StalledWallLegRouteIO : public GridSimRouteIO{
public:
    StalledWallLegRouteIO(GridSim& sim, std::vector<std::string>& notes)
        : GridSimRouteIO(sim, notes)
    {}

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override{
        if (m_first_wall_call){
            m_first_wall_call = false;
            return WalkResult{
                .stop = WalkStop::LIMIT,
                .walked = 5000ms,
                .wild_battles = 0,
                .moved_chunks = 2,
            };
        }
        return GridSimRouteIO::walk_to_wall(walk);
    }

private:
    bool m_first_wall_call = true;
};

UnitTestResult test_wall_leg_check_at_wall_short_distance_safeguard(Logger& logger){
    StoryWorld world = make_world({
        "########################",
        "#......................#",
        "########################",
    });
    const TilePos start{1, 1};
    const TilePos target{22, 1};
    GridSim sim({.world = &world}, start, Direction::EAST, true);
    std::vector<std::string> notes;
    StalledWallLegRouteIO sim_io(sim, notes);
    RouteTrainers trainers{{}, [](const std::string&){}};
    try{
        walk_route(sim_io, world, {start}, trainers, {
            .goal = [&](const TilePos& pos){ return pos == target; },
            .label = "test_check_at_wall_safeguard",
            .avoid = {},
            .costs = *story_route_costs("town"),
            .speed = MoveSpeed::RUN,
        });
        return "check_at_wall falsely accepted leg stalled well short of expected distance";
    }catch (const GridSimFailure& f){
        if (f.message.find("short of the wall") != std::string::npos){
            return true;
        }
        return "caught unexpected failure: " + f.message;
    }
}
//  Oreburgh Mine B1F: the entrance hall corridor is traversed to and back by
//  running south from (12, 1) to (12, 21), and north from (12, 21), where the
//  coal room's warp lands us, to (12, 1).
UnitTestResult test_oreburgh_mine_hall_corridor(Logger& logger){
    const StoryWorld* world = story_plan_world("OreburghMineB1F", "OreburghMineB1F");
    if (world == nullptr){
        return "OreburghMineB1F world not found.";
    }
    const SimWorld sim_world{.world = world, .can_run = true};
    const auto test_run = [&](TilePos start, Direction dir, TilePos expected, const char* label){
        GridSim sim(sim_world, start, dir, true);
        const SimMove move = sim.hold(dir, 5000ms, SimPace::RUN);
        if (move.stop != SimStop::WARP || sim.player().pos != sim_pos(expected)){
            return std::format("{}: expected WARP at {}, got {} at {}.", label,
                Pokemon::to_string(expected), sim_stop_name(move.stop), Pokemon::to_string(nearest_tile(sim.player().pos)));
        }
        logger.log(std::format("{} hall run reached {} in {} ms.", label, Pokemon::to_string(expected), move.time.count()));
        return std::string{};
    };
    std::string err = test_run({12, 1}, Direction::SOUTH, {12, 21}, "Southbound");
    if (!err.empty()){
        return err;
    }
    err = test_run({12, 21}, Direction::NORTH, {12, 1}, "Northbound");
    return err.empty() ? UnitTestResult(true) : UnitTestResult(err);
}

struct AutoStoryGridSimInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryGridSimExpectation{
};

struct AutoStoryGridSimCase{
    AutoStoryGridSimInput input;
    AutoStoryGridSimExpectation expectation;
};

class Test_GridSim : public UnitTest{
public:
    explicit Test_GridSim(AutoStoryGridSimCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: " + test_case.input.name)
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
    AutoStoryGridSimCase m_case;
};

} // namespace


const AutoStoryGridSimCase AUTO_STORY_GRID_SIM_CASES[] = {
    {
        .input = {"taps, holds, stick offsets, doors, people and trainers as measured", test_moves},
        .expectation = {},
    },
    {
        .input = {"the table's routes end on their targets", test_table_routes},
        .expectation = {},
    },
    {
        .input = {"a stick-tap turn misses the Sandgem Center and Jubilife School doors", test_door_routes},
        .expectation = {},
    },
    {
        .input = {"01.03's old taps across Roark's room miss his rock from a row off (lab-B, 2026-11-08)", test_roarks_room_from_a_row_off},
        .expectation = {},
    },
    {
        .input = {"wandering NPC movement and deterministic seed behavior", test_wandering_npc_mechanics},
        .expectation = {},
    },
    {
        .input = {"wandering NPC collision stops player and opens dialogue", test_wandering_npc_collision_and_dialog},
        .expectation = {},
    },
    {
        .input = {"01.03 above_the_mine paths through wanderer zone across seeds and start times", test_wandering_npc_routes_multi_seed},
        .expectation = {},
    },
    {
        .input = {"wandering NPC collision without avoid waits for clearance and resumes", test_wandering_npc_wait_clearance},
        .expectation = {},
    },
    {
        .input = {"01.03 oreburgh_north paths straight up column 46 across seeds and start times", test_wandering_npc_oreburgh_north_multi_seed},
        .expectation = {},
    },
    {
        .input = {"check_at_wall safeguard rejects legs stalled well short of expected distance", test_wall_leg_check_at_wall_short_distance_safeguard},
        .expectation = {},
    },
    {
        .input = {"01.03, Oreburgh Mine B1F entrance hall corridor runs to and back", test_oreburgh_mine_hall_corridor},
        .expectation = {},
    },
    {
        .input = {"01.05, a part of a tile off, the old open walk west takes the Gate's warp (lab-B, 2026-11-08)", test_gate_walk_part_tile_off},
        .expectation = {},
    },
    {
        .input = {"01.11, the old woman from each tile Floaroma's banner stops us on (lab-B, 2026-11-08)", test_old_couple_from_each_arrival},
        .expectation = {},
    },
};

void add_tests_AutoStory_GridSim(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_GRID_SIM_CASES){
        database.add<Test_GridSim>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
