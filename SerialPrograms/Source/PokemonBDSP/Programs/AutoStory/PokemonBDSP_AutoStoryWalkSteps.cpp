/*  BDSP AutoStory - Story walk steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <iterator>
#include <utility>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/Navigation/Grid/Trace/Pokemon_LegTrace.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_MapData.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryRobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryRoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryTrainerSight.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryWallRecovery.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryCountedWallLeg.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryLocation.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryPlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryRouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkDpad.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkResult.h"
#include "PokemonBDSP_AutoStoryCheckpoint.h"
#include "PokemonBDSP_AutoStoryPlaces.h"
#include "PokemonBDSP_AutoStoryTable.h"
#include "PokemonBDSP_AutoStoryWalkSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::StoryFailure;
using Pokemon::TilePos;


namespace{

//  The leg records of a step: its legs are this event's, and one still
//  open when the step ends is written with its end unknown.
class EventTrace{
public:
    EventTrace(Pokemon::LegTracer& legs, const std::string& row, const std::string& step)
        : m_legs(legs)
    {
        m_legs.begin_event(row, step);
    }
    ~EventTrace(){
        try{
            m_legs.end_event();
        }catch (...){}
    }
    EventTrace(const EventTrace&) = delete;
    EventTrace& operator=(const EventTrace&) = delete;

private:
    Pokemon::LegTracer& m_legs;
};

//  `tiles` of `map` as the route lines name them: by the story world's
//  map names, where the map has a world.
std::vector<Pokemon::RouteTile> traced_tiles(const std::string& map, const std::vector<TilePos>& tiles){
    const StoryWorld* world = story_plan_world(map, map);
    std::vector<Pokemon::RouteTile> out;
    for (const TilePos& pos : tiles){
        const std::optional<MapTile> local = world == nullptr ? std::nullopt : world->to_local(story_to_world(*world, map, pos));
        out.push_back(local ? Pokemon::RouteTile{local->map, local->pos} : Pokemon::RouteTile{map, pos});
    }
    return out;
}

//  How long a scene waits for its first line when the preceding step did not
//  press through its dialogue.
constexpr Milliseconds SCENE_START_WAIT = 15000ms;

//  "to_catch_grass" as "to catch grass", for the logs.
std::string label_of(const BdspStep& step){
    std::string label = step.name;
    std::replace(label.begin(), label.end(), '_', ' ');
    return label;
}

//  Fails the step (fail_step() on the console).
[[noreturn]] void fail(RouteIO& io, const std::string& message){
    io.note(message, RouteNote::FAILURE);
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "RouteIO::note() returned from a failure: " + message);
}

//  A trainer every pass of a row fights, by that row.
struct FoughtTrainer{
    std::string_view row;
    std::string_view name;
};
//  01.02's walk east along Route 203's row 18 crosses his sight down
//  column 31 (route_203_trainer_sights()).
const FoughtTrainer FOUGHT_TRAINERS[] = {
    {"01.02", "Youngster (above row 18)"},
};

//  A map-changing step that triggers an arrival cutscene on its own without a
//  following scene step: its settle must wait out the scene (SettleKind::TALK),
//  not a plain door's short stillness (SettleKind::DOOR).
struct ArrivalSceneDoor{
    std::string_view row;
    std::string_view step;
};
const ArrivalSceneDoor ARRIVAL_SCENE_DOORS[] = {
    //  Dawn's tour of Sandgem Town starts immediately on exiting Rowan's Lab.
    {"00.04", "out_of_the_lab"},
};

} // namespace


std::vector<TrainerSight> row_trainers(const StoryWorld& world, const std::string& row){
    std::vector<TrainerSight> sights = story_trainers(world);
    std::erase_if(sights, [&row](const TrainerSight& sight){
        return std::any_of(std::begin(FOUGHT_TRAINERS), std::end(FOUGHT_TRAINERS), [&](const FoughtTrainer& fought){
            return row > std::string(fought.row) && sight.name == fought.name;
        });
    });
    return sights;
}

TappedEnd tapped_end(const StoryWorld& world, const std::string& map, const TilePos& pos, const Pokemon::StoryLeg& leg){
    const size_t open = slide_to_wall(world, story_to_world(world, map, pos), leg.direction).tiles;
    TappedEnd tapped{.end = pos, .moved = std::min(open, (size_t)leg.tiles)};
    for (size_t i = 0; i < tapped.moved; i++){
        tapped.end = tapped.end.next(leg.direction);
    }
    return tapped;
}

std::optional<GridPose> measured_ends(const GridPose& from, const BdspStep& step){
    const StoryWorld* world = story_plan_world(from.map, from.map);
    const LegsStepArgs& args = std::get<LegsStepArgs>(step.args);
    if (world == nullptr || args.legs.empty()){
        return std::nullopt;
    }
    std::vector<TilePos> at = from.tiles;
    for (const Pokemon::StoryLeg& leg : args.legs){
        std::vector<TilePos> next;
        for (const TilePos& pos : at){
            const TilePos end = tapped_end(*world, from.map, pos, leg).end;
            if (std::find(next.begin(), next.end(), end) == next.end()){
                next.push_back(end);
            }
        }
        at = std::move(next);
    }
    return GridPose{.map = from.map, .tiles = std::move(at), .facing = args.legs.back().direction};
}

void StoryWalkSteps::begin_attempt(const std::string& row, const std::vector<BdspStep>& steps){
    m_row = row;
    //  A row starts from its save: after the shoes' row, the save has them.
    m_can_run = row > std::string(RUNNING_SHOES_ROW);
    m_trainers.clear();
    m_warp = {};
    m_scene_next.clear();
    m_door_scene_steps.clear();
    m_scene_open = false;
    m_previous_settle_saw_dialogue = false;
    m_scene_start_already_played = false;
    m_actual_facing.reset();
    //  A route right before a scene, or before a plain step onto the scene's
    //  trigger (the decomp's tile, which the console may have elsewhere).
    const auto onto_trigger = [](const BdspStep& step){
        return step.action == "step" && std::get<StepMoveArgs>(step.args).variant.empty() && !step.pose_after;
    };
    const auto is_route = [](const BdspStep& step){
        return step.action == "route";
    };
    for (size_t c = 1; c < steps.size(); c++){
        if (steps[c].action != "scene"){
            continue;
        }
        if (is_route(steps[c - 1])){
            m_scene_next.insert(steps[c - 1].name);
        }
        if (steps[c - 1].action == "step" && steps[c - 1].pose_after){
            m_door_scene_steps.insert(steps[c - 1].name);
        }
        if (c >= 2 && onto_trigger(steps[c - 1]) && is_route(steps[c - 2])){
            m_scene_next.insert(steps[c - 2].name);
        }
    }
}

void StoryWalkSteps::begin_step(const BdspStep& step){
    const bool previous_settle_saw_dialogue = std::exchange(m_previous_settle_saw_dialogue, false);
    m_scene_start_already_played = step.action == "scene" && previous_settle_saw_dialogue;
    if (step.action != "route" && step.action != "legs" && step.action != "step" && step.action != "talk"){
        m_actual_facing.reset();
    }
}

void begin_walk_step(StoryWalkSteps& walks, const BdspStep& step){
    walks.begin_step(step);
}

void StoryWalkSteps::turn(Direction direction){
    m_route.note(std::string("[Route] Turning ") + direction_name(direction) + ".", RouteNote::LOG);
    m_route.press(route_turn_tap(direction));
    m_actual_facing = direction;
}

void StoryWalkSteps::log(const std::string& text){
    m_route.note(text, RouteNote::LOG);
}

GridPose StoryWalkSteps::walk_to(const GridPose& from, const GridPose& goal, const BdspStep& step, const std::string& variant){
    const StoryWorld* maps = story_plan_world(from.map, goal.map);
    if (maps == nullptr){
        fail(m_route, std::format("{}: no maps join {} and {}.", step.name, from.map, goal.map));
    }
    const std::optional<RobustPlanCosts> costs = story_route_costs(variant);
    if (!costs){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown route costs: " + variant);
    }
    std::vector<TilePos> start;
    for (const TilePos& pos : from.tiles){
        start.push_back(story_to_world(*maps, from.map, pos));
    }
    std::vector<TilePos> targets;
    for (const TilePos& pos : goal.tiles){
        targets.push_back(story_to_world(*maps, goal.map, pos));
    }
    //  The ones fought stay off m_trainers until the next attempt.
    std::vector<TrainerSight>& sights = m_trainers.try_emplace(maps, row_trainers(*maps, m_row)).first->second;
    RouteTrainers trainers{
        sights,
        [this](const std::string& trainer){ m_io.fight(trainer); }
    };
    const RouteWalkPlan plan{
        [&](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); },
        label_of(step), {}, *costs, m_can_run ? MoveSpeed::RUN : MoveSpeed::WALK, m_scene_next.contains(step.name),
        variant == ROUTE_ONTO_TRIGGER, m_actual_facing ? m_actual_facing : from.facing,
    };
    RouteEnd end = walk_route_to_warp(m_route, *maps, start, trainers, plan);
    sights = std::move(trainers.sights);
    m_warp = std::move(end.warp);
    m_scene_open = end.scene;
    m_actual_facing = (m_warp.any() || m_scene_open) ? std::nullopt : end.facing;
    std::vector<TilePos> local;
    for (const TilePos& pos : end.tiles){
        const std::optional<TilePos> tile = story_to_local(*maps, goal.map, pos);
        if (!tile){
            fail(m_route, std::format("{}: the walk ended off {}.", step.name, goal.map));
        }
        local.push_back(*tile);
    }
    return GridPose{.map = goal.map, .tiles = std::move(local), .facing = goal.facing};
}

std::optional<StoryFailure> StoryWalkSteps::route(BdspRunState& state, const BdspStep& step){
    const EventTrace trace(m_legs, m_row, step.name);
    m_warp = {};
    if (state.pose.tiles.empty()){
        return StoryFailure{.code = "no_pose", .detail = format_grid_pose(state.pose)};
    }
    const RouteStepArgs& args = std::get<RouteStepArgs>(step.args);
    const GridPose end = walk_to(state.pose, args.target, step, args.variant);
    //  The next step's scene started on the way: its step plays it.
    if (args.legs.empty() || m_scene_open){
        state.pose = end;
        return std::nullopt;
    }
    return run_to_dialogue(step, args.legs);
}

//  The run of a route step with a leg, after its route: on toward the leg's
//  direction until a dialogue starts, wherever on the run it starts (a
//  scene's trigger: r35/r36 raced Dawn's to an exact tile and banner), for
//  the leg's tiles at most. A wild battle on the way is run from and the run
//  goes on. No dialogue fails the step. The scene's lines are the next step's.
//  The leg's tiles are timed at the slowest pace of the player's speed (a
//  walk before the Running Shoes): a run past the trigger's tile is ended by
//  its dialogue anyway, where one cut short fails (lab-B 2026-11-09, 00.03).
std::optional<StoryFailure> StoryWalkSteps::run_to_dialogue(const BdspStep& step, const std::vector<Pokemon::StoryLeg>& legs_list){
    const Pokemon::StoryLeg& leg = legs_list.front();
    const MoveSpeed speed = m_can_run ? MoveSpeed::RUN : MoveSpeed::WALK;
    m_legs.begin_leg({.direction = leg.direction, .tiles = (size_t)leg.tiles, .end_kind = Pokemon::LegEndKind::OPEN});
    const WalkResult walk = m_route.walk_to_wall({
        .direction = leg.direction,
        .limit = route_limit_for_tiles(speed, leg.tiles),
        .label = label_of(step),
        .options = {.arrived = nullptr, .pace = m_can_run ? DpadPace::RUN : DpadPace::WALK},
    });
    log(std::format("[{}] {}: ran {} to a dialogue: {} moved chunks, {} tiles at most, {}, {} wild battle(s).", m_row, step.name,
        direction_name(leg.direction), walk.moved_chunks, leg.tiles, walk_stop_name(walk.stop), walk.wild_battles));
    m_legs.end_leg({}, walk.stop == WalkStop::DIALOG ? Pokemon::LegEndKnown::DIALOGUE : Pokemon::LegEndKnown::UNKNOWN);
    if (walk.stop == WalkStop::DIALOG){
        return std::nullopt;
    }
    //  A scene can lock us before its first line: Dawn walks over first (r37).
    if (m_route.dialog_within(10000ms, COLOR_CYAN)){
        return std::nullopt;
    }
    return StoryFailure{
        .code = "no_dialogue",
        .detail = std::format("{}: no dialogue in {} tiles {}", step.name, leg.tiles, direction_name(leg.direction)),
    };
}

//  A legs step "to_map": runs its one leg until the location banner of
//  pose_after's map shows, wherever it shows. A straight run along an open
//  row needs no tile count (r41, r42: Sandgem's row 11 took us into the lab).
std::optional<StoryFailure> StoryWalkSteps::leg_to_map(const BdspStep& step){
    const LegsStepArgs& args = std::get<LegsStepArgs>(step.args);
    const Pokemon::StoryLeg& leg = args.legs.front();
    const std::string map = step.pose_after ? step.pose_after->map : "";
    turn(leg.direction);
    const bool arrived = m_io.run_until_map(leg.direction, Milliseconds(250) * leg.tiles + 2000ms, map, step.name);
    if (arrived){
        return std::nullopt;
    }
    return StoryFailure{.code = "no_banner", .detail = map};
}

namespace{
//  Taps toward a door with no table door ahead of it.
constexpr size_t TAPS_TO_BANNER = 30;
//  How many taps `direction` a tap step from `from` takes at most: the
//  tiles to the door its map has ahead of every tile of `from`, and two
//  more; TAPS_TO_BANNER where the map has none. A step through already,
//  its banner gone, then stops a few tiles on (lab-B 2026-11-08: 01.05's
//  walk slid into the Oreburgh Gate, whose row 22 runs 24 tiles on).
size_t taps_to_banner(const GridPose& from, Direction direction){
    size_t most = 0;
    try{
        const Pokemon::MapData& map = story_map(from.map);
        for (const TilePos& pos : from.tiles){
            const size_t tiles = Pokemon::tiles_to_door(map, pos, direction);
            TilePos door = pos;
            for (size_t i = 0; i < tiles; i++){
                door = door.next(direction);
            }
            if (tiles == 0 || map.tiles.at(door) != Pokemon::TileKind::DOOR){
                return TAPS_TO_BANNER;
            }
            most = std::max(most, tiles);
        }
    }catch (...){
        return TAPS_TO_BANNER;
    }
    return most == 0 ? TAPS_TO_BANNER : most + 2;
}
} // namespace

//  A step "to_banner": D-pad taps toward a door or cave mouth until the fade
//  and pose_after's map's location banner. No banner fails the step right
//  there; a banner before any tap means the walk before took us through.
std::optional<StoryFailure> StoryWalkSteps::tap_to_banner(const GridPose& from, const BdspStep& step){
    const StepMoveArgs& args = std::get<StepMoveArgs>(step.args);
    const Direction direction = args.direction;
    const std::string map = step.pose_after ? step.pose_after->map : "";
    //  After the way through, the far side may still fade in, and a walk
    //  then goes nowhere (lab-B 2026-11-10, 00.04b's Center): settle, as
    //  after a door.
    if (through_already(step)){
        m_previous_settle_saw_dialogue = m_io.settle(BUTTON_B, label_of(step), SettleKind::DOOR, "");
        return std::nullopt;
    }
    const size_t limit = taps_to_banner(from, direction);
    const std::optional<size_t> taps = m_io.tap_to_banner(direction, map, limit);
    if (!taps){
        return StoryFailure{
            .code = "no_banner",
            .detail = std::format("no fade and no {} banner in {} taps {}", map, limit, direction_name(direction)),
        };
    }
    if (*taps == 0){
        log(std::format("[{}] {}: {}'s banner before any tap: through already.", m_row, step.name, map));
    }else{
        log(std::format("[{}] {}: through to {} after {} taps {}.", m_row, step.name, map, *taps, direction_name(direction)));
    }
    m_previous_settle_saw_dialogue = m_io.settle(BUTTON_B, label_of(step), SettleKind::DOOR, "");
    return std::nullopt;
}


//  A step "or_legs" whose run into the door saw nothing (`seen`): the run
//  stopped beside the door's column, against the wall it is in. The step's
//  legs, a tap a tile, put us on the door's tile, then the run goes again
//  (00.03: from (5, 10) east onto the mat (6, 10), then south). A tap onto
//  the tile that is itself the way through ends it. `seen` itself for
//  another variant, or when the run saw the warp.
WarpSeen StoryWalkSteps::door_from_beside(const BdspStep& step, const WarpSeen& seen, const Pokemon::DoorApproachPlan& door, const std::string& banner){
    const StepMoveArgs& args = std::get<StepMoveArgs>(step.args);
    if (seen.any() || args.variant != "or_legs"){
        return seen;
    }
    for (const Pokemon::StoryLeg& leg : args.legs){
        turn(leg.direction);
        const WalkResult walk = m_route.walk_taps(leg.direction, (size_t)leg.tiles, step.name);
        log(std::format("[{}] {}: no fade; {} {} taps {}, {}.", m_row, step.name, leg.tiles,
            direction_name(leg.direction), walk.moved_chunks, walk_stop_name(walk.stop)));
        if (walk.warp.any()){
            return walk.warp;
        }
    }
    const Direction direction = args.direction;
    Pokemon::DoorApproachPlan retry = door;
    retry.dark_from = Milliseconds(0);
    const WarpSeen again = m_io.run_into_door(direction, m_can_run, retry, banner);
    log(std::format("[{}] {}: {} through the door again: {}.", m_row, step.name,
        direction_name(direction), again.any() ? describe(again) : "nothing yet"));
    return again;
}

//  The route before ran on through this step's warp (walk_route_to_warp()):
//  the step is done, unless the banner it read names another map.
bool StoryWalkSteps::through_already(const BdspStep& step){
    const WarpSeen through = std::exchange(m_warp, {});
    if (!through.any()){
        return false;
    }
    const std::string map = step.pose_after ? step.pose_after->map : "";
    if (!through.banner.empty() && !location_banner_matches(through.banner, story_banner_name(map))){
        fail(m_route, std::format("{}: the walk before went through a warp to \"{}\", not {}.", step.name, through.banner, map));
    }
    log(std::format("[{}] {}: through already; the walk before saw {}.", m_row, step.name, describe(through)));
    return true;
}

SettleKind StoryWalkSteps::door_settle_kind(const BdspStep& step) const{
    if (m_door_scene_steps.contains(step.name)){
        return SettleKind::TALK;
    }
    for (const ArrivalSceneDoor& door : ARRIVAL_SCENE_DOORS){
        if (m_row == door.row && step.name == door.step){
            return SettleKind::TALK;
        }
    }
    return SettleKind::DOOR;
}

namespace{
//  Dark before this share of the run's time to the door is not the door:
//  lab-B 2026-11-08 09:27:33 read dark at 1.8 s of the 4050 ms run down the
//  mine's entrance hall (3250 ms to the mouth), stopped there, and the next
//  step walked on through the mouth.
constexpr double DOOR_DARK_FROM_SHARE = 0.85;
//  The shortest fade watch: the one-tile step onto the mine's warp faded 3.35 s
//  after its hold began (lab-B 2026-11-07 12:19 and 12:27); a 1500 ms watch
//  gave up just before the fade (2026-11-08 09:27:16) and settle() pressed B
//  15 s before it called the overworld calm.
constexpr Milliseconds DOOR_MIN_HOLD = 4500ms;
//  Margin added on top of the farthest candidate tile's travel time before
//  releasing the D-pad automatically, so a delayed fade cancel (capture lag)
//  cannot keep holding past the ~1.5 s warp fade onto the far map.
constexpr Milliseconds DOOR_DRIVE_MARGIN = 500ms;
} // namespace

Pokemon::DoorApproachPlan bdsp_door_approach_plan(
    const std::vector<size_t>& tile_distances,
    Direction direction,
    const BdspDoorApproachOptions& options
){
    const Milliseconds per_tile = route_ms_per_tile(direction, options.can_run ? MoveSpeed::RUN : MoveSpeed::WALK);
    const Pokemon::DoorApproachTiming timing{
        .start_delay = ROUTE_START_DELAY,
        .per_tile = per_tile,
        .drive_margin = DOOR_DRIVE_MARGIN,
        .fade_margin = 800ms,
        .min_fade_wait = DOOR_MIN_HOLD,
        .fallback_timeout = 12000ms,
        .dark_from_share = options.ignore_early_dark ? DOOR_DARK_FROM_SHARE : 0.0,
    };
    return Pokemon::plan_door_approach(tile_distances, timing);
}

namespace{
//  The run through a door from `pose`: timed from the tiles to the door on
//  the story map, or from the step's door_tiles on a map with none (an
//  interior). With neither, the long fallback hold, which ran on past the
//  door onto the far map once the fade's cancel came late (00.05's exit
//  from the Trainers' School, lab-B 2026-11-22: one tile past (40, 41)).
Pokemon::DoorApproachPlan door_run(const std::string& map_name, const GridPose& pose, Direction direction, bool can_run, size_t door_tiles){
    const BdspDoorApproachOptions options{.can_run = can_run, .ignore_early_dark = true};
    if (door_tiles > 0){
        return bdsp_door_approach_plan({door_tiles}, direction, options);
    }
    try{
        const Pokemon::MapData& map = story_map(map_name);
        std::vector<size_t> distances;
        distances.reserve(pose.tiles.size());
        for (const TilePos& tile : pose.tiles){
            distances.push_back(Pokemon::tiles_to_door(map, tile, direction));
        }
        return bdsp_door_approach_plan(distances, direction, options);
    }catch (...){}
    return bdsp_door_approach_plan({}, direction, options);
}
enum class CountedEndPolicy{ EXACT, ONE_SHORT };
std::vector<TilePos> counted_ends(const std::vector<TilePos>& starts, const Pokemon::StoryLeg& leg, CountedEndPolicy policy){
    std::vector<TilePos> ends;
    for (TilePos at : starts){
        for (int i = 0; i < leg.tiles; i++){
            if (policy == CountedEndPolicy::ONE_SHORT && i + 1 == leg.tiles){
                ends.push_back(at);
            }
            at = at.next(leg.direction);
        }
        ends.push_back(at);
    }
    return ends;
}
} // namespace

GridPose StoryWalkSteps::recover_tapped_leg(const GridPose& from, const std::vector<TilePos>& starts,
    const std::vector<TilePos>& goals, Direction facing, const Pokemon::StoryLeg& leg, const BdspStep& step,
    const std::optional<TileSpan>& moved){
    const StoryWorld* world = story_plan_world(from.map, from.map);
    if (world == nullptr){
        fail(m_route, "interrupted leg has no recovery world: " + from.map);
    }
    std::vector<TilePos> global;
    for (const TilePos& at : starts){
        global.push_back(story_to_world(*world, from.map, at));
    }
    std::vector<TilePos> possible;
    //  Placed by the leg's screen events: a tile or so from where the taps
    //  left us. Else anywhere along the leg.
    const std::vector<TilePos> along = moved
        ? tiles_moved(*world, global, leg.direction, *moved)
        : interrupted_leg_positions(*world, global, leg.direction, (size_t)leg.tiles);
    for (const TilePos& at : along){
        const auto local = story_to_local(*world, from.map, at);
        if (!local){
            fail(m_route, "interrupted leg crossed an unconfirmed map transition");
        }
        possible.push_back(*local);
    }
    BdspStep recovery = step;
    recovery.name += "_recovery";
    const GridPose goal{.map = from.map, .tiles = goals, .facing = facing};
    const GridPose found = walk_to(GridPose{.map = from.map, .tiles = std::move(possible)}, goal, recovery, "");
    //  The route back can approach from another direction. Establish the
    //  step's facing before publishing that pose to the story runner.
    turn(facing);
    return found;
}

//  Counted legs retain their normal path. An observed encounter invalidates
//  the exact pose; localize from every legal prefix and re-plan to the end of
//  the step, skipping the legs left. A battle that may have begun before
//  the leg (WalkOrigin::MAY_PRECEDE_LEG) found us on the tile the leg before
//  ended on, since a battle starts on a tile stepped onto: the same prefixes
//  hold. A re-plan to the end of the interrupted
//  leg ended on an open tile, its last leg overshot a row, and the taps after
//  went on a row off (lab-B 2026-11-08 11:55); a step that ends in a corner
//  ends its re-plan on wall clips.
std::optional<StoryFailure> StoryWalkSteps::tap_legs(BdspRunState& state, const BdspStep& step){
    const GridPose& from = state.pose;
    const LegsStepArgs& args = std::get<LegsStepArgs>(step.args);
    const CountedEndPolicy policy = args.variant == "one_short_ok" ? CountedEndPolicy::ONE_SHORT : CountedEndPolicy::EXACT;
    const bool one_short_ok = policy == CountedEndPolicy::ONE_SHORT;
    std::vector<TilePos> at = from.tiles;
    std::optional<Direction> facing = m_actual_facing ? m_actual_facing : from.facing;
    for (auto leg = args.legs.begin(); leg != args.legs.end(); ++leg){
        if (facing && *facing != leg->direction){
            turn(leg->direction);
        }
        facing = leg->direction;
        m_actual_facing = facing;
        const size_t tiles = (size_t)leg->tiles;
        const auto next = counted_ends(at, *leg, policy);
        m_legs.begin_leg({.direction = leg->direction, .tiles = tiles, .end_kind = Pokemon::LegEndKind::TILE,
            .ends = traced_tiles(from.map, next)});
        const WalkResult walk = m_route.walk_taps(leg->direction, tiles, step.name);
        log(std::format("[{}] {}: taps {} moved {} of {} tiles.", m_row, step.name, direction_name(leg->direction), walk.moved_chunks, tiles));
        if (walk.wild_battles > 0){
            if (walk.stop != WalkStop::LIMIT && walk.stop != WalkStop::BLOCKED){
                return StoryFailure{.code = "interrupted_leg_unconfirmed", .detail = step.name};
            }
            const LegBattlePlace place = place_leg_battle(walk, tiles);
            m_legs.end_leg({}, Pokemon::LegEndKnown::UNKNOWN, "recovery");
            m_route.note(std::format("[{}] {}", m_row, leg_battle_line(step.name, walk, place)), RouteNote::LOG);
            std::vector<TilePos> ends = next;
            for (auto rest = std::next(leg); rest != args.legs.end(); ++rest){
                ends = counted_ends(ends, *rest, policy);
            }
            state.pose = recover_tapped_leg(from, at, ends, args.legs.back().direction, *leg, step, place.moved);
            return std::nullopt;
        }
        if (walk.moved_chunks + (one_short_ok ? 1 : 0) < tiles){
            return StoryFailure{
                .code = "short_leg",
                .detail = std::format("{} {} moved {}", direction_name(leg->direction), tiles, walk.moved_chunks),
            };
        }
        at = next;
        m_legs.end_leg(traced_tiles(from.map, at), Pokemon::LegEndKnown::COUNTED);
    }
    state.pose = GridPose{.map = from.map, .tiles = std::move(at), .facing = args.legs.back().direction};
    return std::nullopt;
}

//  A legs step "measured": each leg tapped, its taps that moved counted
//  against the map's from each tile the player may stand on; the tiles
//  whose count matches are where we are. A wall a tap bumps tells us
//  where we are, where a fixed leg from a guessed tile walks on astray
//  (00.03, lab-B 2026-11-09: a row low, the taps crossed the door's mat).
std::optional<StoryFailure> StoryWalkSteps::measured_legs(BdspRunState& state, const BdspStep& step){
    const GridPose& from = state.pose;
    const StoryWorld* world = story_plan_world(from.map, from.map);
    std::vector<TilePos> at = from.tiles;
    if (world == nullptr || at.empty()){
        return StoryFailure{.code = "measured", .detail = "no map or tiles: " + format_grid_pose(from)};
    }
    const LegsStepArgs& args = std::get<LegsStepArgs>(step.args);
    std::optional<Direction> facing = m_actual_facing ? m_actual_facing : from.facing;
    for (const Pokemon::StoryLeg& leg : args.legs){
        if (facing && *facing != leg.direction){
            turn(leg.direction);
        }
        facing = leg.direction;
        m_actual_facing = facing;
        const size_t tiles = (size_t)leg.tiles;
        m_legs.begin_leg({.direction = leg.direction, .tiles = tiles, .end_kind = Pokemon::LegEndKind::TILE});
        const WalkResult walk = m_route.walk_taps(leg.direction, tiles, step.name);
        if (walk.wild_battles > 0){
            return StoryFailure{.code = "interrupted_leg_unconfirmed", .detail = step.name};
        }
        std::vector<TilePos> next;
        for (const TilePos& pos : at){
            const TappedEnd tapped = tapped_end(*world, from.map, pos, leg);
            if (tapped.moved == walk.moved_chunks){
                next.push_back(tapped.end);
            }
        }
        log(std::format("[{}] {}: taps {} moved {} of {}: {} of {} tiles fit.", m_row, step.name,
            direction_name(leg.direction), walk.moved_chunks, tiles, next.size(), at.size()));
        if (next.empty()){
            return StoryFailure{
                .code = "measured",
                .detail = std::format("{} moved {}: no tile of {} fits", direction_name(leg.direction), walk.moved_chunks, format_grid_pose(from)),
            };
        }
        at = std::move(next);
        m_legs.end_leg(traced_tiles(from.map, at), Pokemon::LegEndKnown::COUNTED);
    }
    state.pose = GridPose{.map = from.map, .tiles = std::move(at), .facing = args.legs.back().direction};
    return std::nullopt;
}

//  A legs step "counted_wall": its one leg held to the wall, its tiles
//  counted against the map's from each tile the table may have us on
//  (walk_counted_wall_leg()). The leg's tiles are the table's reading of
//  the map, and must be among them.
std::optional<StoryFailure> StoryWalkSteps::counted_wall(const GridPose& from, const BdspStep& step){
    const LegsStepArgs& args = std::get<LegsStepArgs>(step.args);
    const StoryWorld* world = story_plan_world(from.map, from.map);
    const std::vector<TilePos>& starts = from.tiles;
    if (args.legs.size() != 1 || world == nullptr || starts.empty()){
        return StoryFailure{
            .code = "counted_wall",
            .detail = std::format("{} legs from {}", args.legs.size(), format_grid_pose(from)),
        };
    }
    const Pokemon::StoryLeg& leg = args.legs.front();
    size_t fewest = SIZE_MAX;
    size_t most = 0;
    for (const TilePos& pos : starts){
        const size_t tiles = slide_to_wall(*world, story_to_world(*world, from.map, pos), leg.direction).tiles;
        fewest = std::min(fewest, tiles);
        most = std::max(most, tiles);
    }
    if ((size_t)leg.tiles < fewest || (size_t)leg.tiles > most){
        return StoryFailure{
            .code = "map_mismatch",
            .detail = std::format("{} {} where the map has {} to {}", direction_name(leg.direction), leg.tiles, fewest, most),
        };
    }
    turn(leg.direction);
    m_legs.begin_leg({.direction = leg.direction, .tiles = (size_t)leg.tiles, .end_kind = Pokemon::LegEndKind::WALL});
    walk_counted_wall_leg(m_route, {.direction = leg.direction, .fewest = fewest, .most = most,
        .speed = m_can_run ? MoveSpeed::RUN : MoveSpeed::WALK, .label = label_of(step)});
    m_legs.end_leg({}, Pokemon::LegEndKnown::WALL);
    return std::nullopt;
}

std::optional<StoryFailure> StoryWalkSteps::legs(BdspRunState& state, const BdspStep& step){
    const EventTrace trace(m_legs, m_row, step.name);
    m_warp = {};
    m_scene_open = false;
    const LegsStepArgs& args = std::get<LegsStepArgs>(step.args);
    if (args.variant == "counted_wall"){
        return counted_wall(state.pose, step);
    }
    if (args.variant == "measured"){
        return measured_legs(state, step);
    }
    if (args.variant == "to_wall"){
        //  The walls end these legs; the step's pose_after says where.
        for (const Pokemon::StoryLeg& leg : args.legs){
            m_legs.begin_leg({.direction = leg.direction, .tiles = (size_t)leg.tiles, .end_kind = Pokemon::LegEndKind::WALL});
            m_io.walk_until_stopped(leg.direction, (size_t)leg.tiles, label_of(step));
            m_legs.end_leg({}, Pokemon::LegEndKnown::WALL);
        }
        m_actual_facing = args.legs.empty() ? std::nullopt : std::optional<Direction>(args.legs.back().direction);
        return std::nullopt;
    }
    if (args.variant == "to_map"){
        m_actual_facing.reset();
        return leg_to_map(step);
    }
    if (args.variant == "to_dialogue"){
        m_actual_facing.reset();
        return run_to_dialogue(step, args.legs);
    }
    if (args.variant != "one_short_ok" && !args.variant.empty() && args.variant != "legs"){
        return StoryFailure{.code = "unknown_action", .detail = args.variant};
    }
    return tap_legs(state, step);
}

std::optional<StoryFailure> StoryWalkSteps::step(BdspRunState& state, const BdspStep& step){
    const EventTrace trace(m_legs, m_row, step.name);
    const StepMoveArgs& args = std::get<StepMoveArgs>(step.args);
    const Direction direction = args.direction;
    if (args.variant == "to_banner"){
        turn(direction);
        m_actual_facing.reset();
        return tap_to_banner(state.pose, step);
    }
    if (args.variant == "tap"){
        //  No separate turn tap first: at 1 tile from the door, a turn tap
        //  steps onto the warp, causing walk_taps to take an extra step on the far map.
        const WalkResult walk = m_route.walk_taps(direction, 1, step.name);
        if (walk.wild_battles > 0){
            return StoryFailure{.code = "interrupted_step", .detail = step.name};
        }
        m_actual_facing.reset();
        m_previous_settle_saw_dialogue = m_io.settle(BUTTON_B, label_of(step), SettleKind::TALK, "");
        return std::nullopt;
    }
    //  A step through a door: walk (or run) on until the screen goes dark,
    //  from next to the door or from anywhere in its line (r43: tapping 8 tiles
    //  down the lab took 7 s), then settle. Plain doors with no arrival scene
    //  use SettleKind::DOOR (1 s player-still / 2.5 s calm) once the warp is
    //  seen; arrival-scene doors (Mom's, 00.01; Barry's stairs, 00.01; Dawn's
    //  tour, 00.04) keep SettleKind::TALK. Without the fade, settle must see it
    //  itself (r33 was outside, "no fade"). No separate turn tap first: at 1 tile
    //  from the door, a 50 ms turn tap can step in before run_into_door() watches.
    //  A fade or the far map's banner is the way through, either one (#268
    //  slice 1); with neither yet, settle() waits for one.
    if (step.pose_after && step.pose_after->map != state.pose.map){
        m_actual_facing.reset();
        if (through_already(step)){
            m_previous_settle_saw_dialogue = m_io.settle(BUTTON_B, label_of(step), door_settle_kind(step), "");
            return std::nullopt;
        }
        const std::string banner = story_arrival_banner(state.pose.map, step.pose_after->map);
        const Pokemon::DoorApproachPlan door = door_run(state.pose.map, state.pose, direction, m_can_run, args.door_tiles);
        WarpSeen seen = m_io.run_into_door(direction, m_can_run, door, banner);
        log(std::format("[{}] {}: {} through the door (drive {} ms, timeout {} ms, dark from {} ms): {}.", m_row, step.name,
            direction_name(direction), door.drive_hold.count(), door.fade_timeout.count(), door.dark_from.count(),
            seen.any() ? describe(seen) : "nothing yet"));
        seen = door_from_beside(step, seen, door, banner);
        const SettleKind settle_kind = seen.any() ? door_settle_kind(step) : SettleKind::AFTER_WARP;
        m_previous_settle_saw_dialogue = m_io.settle(BUTTON_B, label_of(step), settle_kind, banner);
        return std::nullopt;
    }
    return one_step(state, step);
}

//  A plain D-pad step: onto a scene's trigger, mostly. Not taken when the
//  route before ran into that scene already (RouteEnd::scene): it holds us
//  where it started.
std::optional<StoryFailure> StoryWalkSteps::one_step(BdspRunState& state, const BdspStep& step){
    if (std::exchange(m_scene_open, false)){
        log(std::format("[{}] {}: its scene started on the walk before; no step.", m_row, step.name));
        return std::nullopt;
    }
    const Direction direction = std::get<StepMoveArgs>(step.args).direction;
    const std::optional<Direction> facing = m_actual_facing ? m_actual_facing : state.pose.facing;
    if (facing && *facing != direction){
        turn(direction);
    }
    m_actual_facing = direction;
    WalkResult walk = m_route.walk_taps(direction, 1, step.name);
    if (walk.wild_battles > 0){
        return StoryFailure{.code = "interrupted_step", .detail = step.name};
    }
    if (walk.moved_chunks == 0 && walk.stop == WalkStop::LIMIT){
        //  A single tap when real facing differed from state.pose (e.g. 01.07/3
        //  rowans_trigger after 01.06/4 ended facing east while 01.07 declares
        //  north) can turn in place without stepping. Now facing `direction`,
        //  take the actual step.
        log(std::format("[{}] {}: first tap {} did not move (turned in place); stepping again.",
            m_row, step.name, direction_name(direction)));
        walk = m_route.walk_taps(direction, 1, step.name);
        if (walk.wild_battles > 0){
            return StoryFailure{.code = "interrupted_step", .detail = step.name};
        }
    }
    log(std::format("[{}] {}: a step {}, {}.", m_row, step.name, direction_name(direction), walk_stop_name(walk.stop)));
    std::vector<TilePos> at;
    for (const TilePos& pos : state.pose.tiles){
        at.push_back(pos.next(direction));
    }
    state.pose = GridPose{.map = state.pose.map, .tiles = std::move(at), .facing = direction};
    return std::nullopt;
}

std::optional<StoryFailure> StoryWalkSteps::talk(BdspRunState& state, const BdspStep& step){
    m_warp = {};
    m_scene_open = false;
    //  Face whom we talk to: a route ends facing its last leg, not its goal
    //  (r40 reached Mom facing north). They block the tile, so the tap only turns.
    if (state.pose.facing){
        turn(*state.pose.facing);
    }
    m_route.press({.kind = RouteInputKind::BUTTON_A, .direction = Direction::NORTH, .hold = 80ms, .release = 200ms,
        .count = 1, .end = RouteInputEnd::DONE});
    //  Roark's first line in the mine comes after the camera zooms in on
    //  us, which can take some seconds (r98; the old event waited 8 s).
    if (!m_route.dialog_within(8000ms, COLOR_YELLOW)){
        fail(m_route, label_of(step) + ": nobody answered.");
    }
    return std::nullopt;
}

std::optional<StoryFailure> StoryWalkSteps::mash(BdspRunState& state, const BdspStep& step){
    m_warp = {};
    m_scene_open = false;
    const SceneStepArgs& args = std::get<SceneStepArgs>(step.args);
    const bool already_played = std::exchange(m_scene_start_already_played, false);
    if (already_played){
        m_route.note(std::format(
            "{}: dialogue was played by the previous step; skipping the opening-line wait.", label_of(step)
        ), RouteNote::STATUS);
    }else if (!m_route.dialog_within(SCENE_START_WAIT, COLOR_CYAN)){
        m_route.note(std::format(
            "{}: no line within {} ms; the step before may already have played it.",
            label_of(step), SCENE_START_WAIT.count()
        ), RouteNote::WARNING);
    }
    const bool cross_world_warp = step.pose_after && step.pose_after->map != state.pose.map &&
        story_plan_world(state.pose.map, step.pose_after->map) == nullptr;
    m_previous_settle_saw_dialogue = m_io.settle(
        args.press_a ? BUTTON_A : BUTTON_B, label_of(step),
        cross_world_warp ? SettleKind::AFTER_WARP : SettleKind::TALK, ""
    );
    if (m_row == RUNNING_SHOES_ROW && step.name == RUNNING_SHOES_STEP){
        m_can_run = true;
    }
    return std::nullopt;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
