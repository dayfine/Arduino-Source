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
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_GridSim_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::story_arg;
using Pokemon::Pose;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;


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
    try{
        out.believed = walk_route(io, *route.world, route.from, trainers, {
            .goal = [&targets](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); },
            .label = route.label,
            .avoid = {},
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
        const GridPose& target = story_arg<GridPose>(step.args, "target");
        next = Pokemon::pose_of(target);
    }else if (at && step.kind == StoryStepKind::STEP){
        const Direction direction = story_arg<Direction>(step.args, "direction");
        std::vector<TilePos> tiles;
        for (const TilePos& pos : Pokemon::tiles_of(*at)){
            tiles.push_back(pos.next(direction));
        }
        next = Pokemon::tile_pose(at->map, tiles, direction);
    }else if (at && step.kind == StoryStepKind::LEGS && (step.action.empty() || step.action == "legs" || step.action == "open")){
        std::vector<TilePos> tiles = Pokemon::tiles_of(*at);
        const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
        for (const Pokemon::StoryLeg& leg : legs_list){
            for (TilePos& pos : tiles){
                for (int i = 0; i < leg.tiles; i++){
                    pos = pos.next(leg.direction);
                }
            }
        }
        next = Pokemon::tile_pose(at->map, tiles, legs_list.back().direction);
    }else if (step.kind != StoryStepKind::GAME && step.kind != StoryStepKind::SHOP &&
        step.kind != StoryStepKind::MASH && step.kind != StoryStepKind::LEGS
    ){
        next = at;
    }
    return step.pose_after ? std::optional<Pose>(Pokemon::pose_of(*step.pose_after)) : next;
}

//  Every ROUTE step of the table that starts from a known pose, from its
//  first and its last tile, at the pace StoryExecutor walks it.
std::vector<SimRoute> table_routes(){
    std::vector<SimRoute> routes;
    for (const StoryCheckpoint& row : story_checkpoints()){
        std::optional<Pose> at = row.start ? std::optional<Pose>(Pokemon::pose_of(*row.start)) : std::nullopt;
        bool shoes = row.id > std::string(RUNNING_SHOES_ROW);
        for (const StoryStep& step : row.steps){
            const GridPose* target_ptr = step.kind == StoryStepKind::ROUTE ? &story_arg<GridPose>(step.args, "target") : nullptr;
            const StoryWorld* world = at && target_ptr ? story_plan_world(at->map, target_ptr->map) : nullptr;
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
                    .targets = world_tiles(*world, Pokemon::pose_of(*target_ptr)), .costs = *story_route_costs(step.action), .can_run = shoes});
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
};

void add_tests_AutoStory_GridSim(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_GRID_SIM_CASES){
        database.add<Test_GridSim>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
