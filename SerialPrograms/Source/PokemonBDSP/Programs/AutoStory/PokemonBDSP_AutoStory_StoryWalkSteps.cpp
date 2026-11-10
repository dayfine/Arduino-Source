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
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/Navigation/Pokemon_LegTrace.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::story_arg;
using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::RunState;
using Pokemon::StoryStep;
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

} // namespace


namespace{

//  "to_catch_grass" as "to catch grass", for the logs.
std::string label_of(const StoryStep& step){
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

CenterEntry run_into_center(StoryStepIO& io, bool run, const std::function<bool()>& inside, const std::function<void()>& line_up){
    if (io.run_into_door(Direction::NORTH, run).any()){
        return CenterEntry::FADED;
    }
    if (inside()){
        return CenterEntry::INSIDE;
    }
    line_up();
    return CenterEntry::LINED_UP;
}

TappedEnd tapped_end(const StoryWorld& world, const std::string& map, const TilePos& pos, const Pokemon::StoryLeg& leg){
    const size_t open = slide_to_wall(world, story_to_world(world, map, pos), leg.direction).tiles;
    TappedEnd tapped{.end = pos, .moved = std::min(open, (size_t)leg.tiles)};
    for (size_t i = 0; i < tapped.moved; i++){
        tapped.end = tapped.end.next(leg.direction);
    }
    return tapped;
}

std::optional<Pose> measured_ends(const Pose& from, const StoryStep& step){
    const StoryWorld* world = story_plan_world(from.map, from.map);
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
    if (world == nullptr || legs_list.empty()){
        return std::nullopt;
    }
    std::vector<TilePos> at = Pokemon::tiles_of(from);
    for (const Pokemon::StoryLeg& leg : legs_list){
        std::vector<TilePos> next;
        for (const TilePos& pos : at){
            const TilePos end = tapped_end(*world, from.map, pos, leg).end;
            if (std::find(next.begin(), next.end(), end) == next.end()){
                next.push_back(end);
            }
        }
        at = std::move(next);
    }
    return Pokemon::tile_pose(from.map, at, legs_list.back().direction);
}

void StoryWalkSteps::begin_attempt(const std::string& row, const std::vector<StoryStep>& steps){
    m_row = row;
    //  A row starts from its save: after the shoes' row, the save has them.
    m_can_run = row > std::string(RUNNING_SHOES_ROW);
    m_trainers.clear();
    m_warp = {};
    m_scene_next.clear();
    m_scene_open = false;
    //  A route right before a scene, or before a plain step onto the scene's
    //  trigger (the decomp's tile, which the console may have elsewhere).
    const auto onto_trigger = [](const StoryStep& step){
        return step.kind == Pokemon::StoryStepKind::STEP && step.action.empty() && !step.pose_after;
    };
    const auto route = [](const StoryStep& step){
        return step.kind == Pokemon::StoryStepKind::ROUTE;
    };
    for (size_t c = 1; c < steps.size(); c++){
        if (steps[c].kind != Pokemon::StoryStepKind::MASH){
            continue;
        }
        if (route(steps[c - 1])){
            m_scene_next.insert(steps[c - 1].name);
        }
        if (c >= 2 && onto_trigger(steps[c - 1]) && route(steps[c - 2])){
            m_scene_next.insert(steps[c - 2].name);
        }
    }
}

void StoryWalkSteps::turn(Direction direction){
    m_route.note(std::string("[Route] Turning ") + direction_name(direction) + ".", RouteNote::LOG);
    m_route.press(route_turn_tap(direction));
}

void StoryWalkSteps::log(const std::string& text){
    m_route.note(text, RouteNote::LOG);
}

Pose StoryWalkSteps::walk_to(const Pose& from, const Pose& goal, const StoryStep& step){
    const StoryWorld* maps = story_plan_world(from.map, goal.map);
    if (maps == nullptr){
        fail(m_route, std::format("{}: no maps join {} and {}.", step.name, from.map, goal.map));
    }
    const std::optional<RobustPlanCosts> costs = story_route_costs(step.action);
    if (!costs){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown route costs: " + step.action);
    }
    std::vector<TilePos> start;
    for (const TilePos& pos : Pokemon::tiles_of(from)){
        start.push_back(story_to_world(*maps, from.map, pos));
    }
    std::vector<TilePos> targets;
    for (const TilePos& pos : Pokemon::tiles_of(goal)){
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
        step.action == ROUTE_ONTO_TRIGGER,
    };
    RouteEnd end = walk_route_to_warp(m_route, *maps, start, trainers, plan);
    sights = std::move(trainers.sights);
    m_warp = std::move(end.warp);
    m_scene_open = end.scene;
    std::vector<TilePos> local;
    for (const TilePos& pos : end.tiles){
        const std::optional<TilePos> tile = story_to_local(*maps, goal.map, pos);
        if (!tile){
            fail(m_route, std::format("{}: the walk ended off {}.", step.name, goal.map));
        }
        local.push_back(*tile);
    }
    return Pokemon::tile_pose(goal.map, local, Pokemon::facing_of(goal));
}

Outcome StoryWalkSteps::route(RunState& state, const StoryStep& step){
    const EventTrace trace(m_legs, m_row, step.name);
    m_warp = {};
    if (Pokemon::tiles_of(state.pose).empty()){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    const GridPose& target = story_arg<GridPose>(step.args, "target");
    const Pose end = walk_to(state.pose, Pokemon::pose_of(target), step);
    const auto legs_list = step.args.contains("legs") ? story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs") : std::vector<Pokemon::StoryLeg>{};
    //  The next step's scene started on the way: its step plays it.
    if (legs_list.empty() || m_scene_open){
        return Pokemon::outcome_ok(end);
    }
    return run_to_dialogue(step);
}

//  The run of a ROUTE step with a leg, after its route: on toward the leg's
//  direction until a dialogue starts, wherever on the run it starts (a
//  scene's trigger: r35/r36 raced Dawn's to an exact tile and banner), for
//  the leg's tiles at most. A wild battle on the way is run from and the run
//  goes on. No dialogue fails the step. The scene's lines are the next step's.
//  The leg's tiles are timed at the slowest pace of the player's speed (a
//  walk before the Running Shoes): a run past the trigger's tile is ended by
//  its dialogue anyway, where one cut short fails (lab-B 2026-11-09, 00.03).
Outcome StoryWalkSteps::run_to_dialogue(const StoryStep& step){
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
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
        return Pokemon::outcome_ok();
    }
    //  A scene can lock us before its first line: Dawn walks over first (r37).
    if (m_route.dialog_within(10000ms, COLOR_CYAN)){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_failed("no_dialogue", std::format("{}: no dialogue in {} tiles {}", step.name, leg.tiles, direction_name(leg.direction)));
}

//  A LEGS step "to_map": runs its one leg until the location banner of
//  pose_after's map shows, wherever it shows. A straight run along an open
//  row needs no tile count (r41, r42: Sandgem's row 11 took us into the lab).
Outcome StoryWalkSteps::leg_to_map(const StoryStep& step){
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
    const Pokemon::StoryLeg& leg = legs_list.front();
    const std::string map = step.pose_after ? step.pose_after->map : "";
    turn(leg.direction);
    const bool arrived = m_io.run_until_map(leg.direction, Milliseconds(250) * leg.tiles + 2000ms, map, step.name);
    return arrived ? Pokemon::outcome_ok() : Pokemon::outcome_failed("no_banner", map);
}

namespace{
//  Taps toward a door with no table door ahead of it.
constexpr size_t TAPS_TO_BANNER = 30;
//  How many taps `direction` a tap step from `from` takes at most: the
//  tiles to the door its map has ahead of every tile of `from`, and two
//  more; TAPS_TO_BANNER where the map has none. A step through already,
//  its banner gone, then stops a few tiles on (lab-B 2026-11-08: 01.05's
//  walk slid into the Oreburgh Gate, whose row 22 runs 24 tiles on).
size_t taps_to_banner(const Pose& from, Direction direction){
    size_t most = 0;
    try{
        const Pokemon::MapData& map = story_map(from.map);
        for (const TilePos& pos : Pokemon::tiles_of(from)){
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

//  A STEP "to_banner": D-pad taps toward a door or cave mouth until the fade
//  and pose_after's map's location banner. No banner fails the step right
//  there; a banner before any tap means the walk before took us through.
Outcome StoryWalkSteps::tap_to_banner(const Pose& from, const StoryStep& step){
    const Direction direction = story_arg<Direction>(step.args, "direction");
    const std::string map = step.pose_after ? step.pose_after->map : "";
    if (through_already(step)){
        return Pokemon::outcome_ok();
    }
    const size_t limit = taps_to_banner(from, direction);
    const std::optional<size_t> taps = m_io.tap_to_banner(direction, map, limit);
    if (!taps){
        return Pokemon::outcome_failed("no_banner", std::format("no fade and no {} banner in {} taps {}",
            map, limit, direction_name(direction)));
    }
    if (*taps == 0){
        log(std::format("[{}] {}: {}'s banner before any tap: through already.", m_row, step.name, map));
    }else{
        log(std::format("[{}] {}: through to {} after {} taps {}.", m_row, step.name, map, *taps, direction_name(direction)));
    }
    return Pokemon::outcome_ok();
}

//  A STEP "or_legs" whose run into the door saw nothing (`seen`): the run
//  stopped beside the door's column, against the wall it is in. The step's
//  legs, a tap a tile, put us on the door's tile, then the run goes again
//  (00.03: from (5, 10) east onto the mat (6, 10), then south). A tap onto
//  the tile that is itself the way through ends it. `seen` itself for
//  another variant, or when the run saw the warp.
WarpSeen StoryWalkSteps::door_from_beside(const StoryStep& step, const WarpSeen& seen, Milliseconds timeout, const std::string& banner){
    if (seen.any() || step.action != "or_legs"){
        return seen;
    }
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
    for (const Pokemon::StoryLeg& leg : legs_list){
        turn(leg.direction);
        const WalkResult walk = m_route.walk_taps(leg.direction, (size_t)leg.tiles, step.name);
        log(std::format("[{}] {}: no fade; {} {} taps {}, {}.", m_row, step.name, leg.tiles,
            direction_name(leg.direction), walk.moved_chunks, walk_stop_name(walk.stop)));
        if (walk.warp.any()){
            return walk.warp;
        }
    }
    const Direction direction = story_arg<Direction>(step.args, "direction");
    const WarpSeen again = m_io.run_into_door(direction, m_can_run, timeout, Milliseconds(0), banner);
    log(std::format("[{}] {}: {} through the door again: {}.", m_row, step.name,
        direction_name(direction), again.any() ? describe(again) : "nothing yet"));
    return again;
}

//  The route before ran on through this step's warp (walk_route_to_warp()):
//  the step is done, unless the banner it read names another map.
bool StoryWalkSteps::through_already(const StoryStep& step){
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

namespace{
//  A run into a door: how long to hold, and for how long of it a dark screen
//  is not yet the door's fade (run_into_door()).
struct DoorRun{
    Milliseconds timeout;
    Milliseconds dark_from;
};
//  Dark before this share of the run's time to the door is not the door:
//  lab-B 2026-11-08 09:27:33 read dark at 1.8 s of the 4050 ms run down the
//  mine's entrance hall (3250 ms to the mouth), stopped there, and the next
//  step walked on through the mouth.
constexpr double DOOR_DARK_FROM_SHARE = 0.85;
//  The shortest hold: the one-tile step onto the mine's warp faded 3.35 s
//  after its hold began (lab-B 2026-11-07 12:19 and 12:27); a 1500 ms hold
//  gave up just before the fade (2026-11-08 09:27:16) and settle() pressed B
//  15 s before it called the overworld calm.
constexpr Milliseconds DOOR_MIN_HOLD = 4500ms;
DoorRun door_run(const std::string& map_name, const Pose& pose, Direction direction, bool can_run){
    try{
        const Pokemon::MapData& map = story_map(map_name);
        const auto tiles_list = Pokemon::tiles_of(pose);
        if (!tiles_list.empty()){
            const size_t tiles = Pokemon::tiles_to_door(map, tiles_list.front(), direction);
            if (tiles > 0){
                const Milliseconds per_tile = route_ms_per_tile(direction, can_run ? MoveSpeed::RUN : MoveSpeed::WALK);
                const Milliseconds to_door = ROUTE_START_DELAY + per_tile * (int64_t)tiles;
                return {
                    .timeout = std::max(DOOR_MIN_HOLD, to_door + 800ms),
                    .dark_from = std::chrono::duration_cast<Milliseconds>(to_door * DOOR_DARK_FROM_SHARE),
                };
            }
        }
    }catch (...){}
    return {.timeout = 12000ms, .dark_from = 0ms};
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

Pose StoryWalkSteps::recover_tapped_leg(const Pose& from, const std::vector<TilePos>& starts,
    const std::vector<TilePos>& goals, Direction facing, const Pokemon::StoryLeg& leg, const StoryStep& step,
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
    StoryStep recovery = step;
    recovery.name += "_recovery";
    recovery.action.clear();
    const Pose found = walk_to(Pokemon::tile_pose(from.map, possible), Pokemon::tile_pose(from.map, goals, facing), recovery);
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
Outcome StoryWalkSteps::tap_legs(const Pose& from, const StoryStep& step){
    const CountedEndPolicy policy = step.action == "one_short_ok" ? CountedEndPolicy::ONE_SHORT : CountedEndPolicy::EXACT;
    const bool one_short_ok = policy == CountedEndPolicy::ONE_SHORT;
    std::vector<TilePos> at = Pokemon::tiles_of(from);
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
    for (auto leg = legs_list.begin(); leg != legs_list.end(); ++leg){
        turn(leg->direction);
        const size_t tiles = (size_t)leg->tiles;
        const auto next = counted_ends(at, *leg, policy);
        m_legs.begin_leg({.direction = leg->direction, .tiles = tiles, .end_kind = Pokemon::LegEndKind::TILE,
            .ends = traced_tiles(from.map, next)});
        const WalkResult walk = m_route.walk_taps(leg->direction, tiles, step.name);
        log(std::format("[{}] {}: taps {} moved {} of {} tiles.", m_row, step.name, direction_name(leg->direction), walk.moved_chunks, tiles));
        if (walk.wild_battles > 0){
            if (walk.stop != WalkStop::LIMIT && walk.stop != WalkStop::BLOCKED){
                return Pokemon::outcome_failed("interrupted_leg_unconfirmed", step.name);
            }
            const LegBattlePlace place = place_leg_battle(walk, tiles);
            m_legs.end_leg({}, Pokemon::LegEndKnown::UNKNOWN, "recovery");
            m_route.note(std::format("[{}] {}", m_row, leg_battle_line(step.name, walk, place)), RouteNote::LOG);
            std::vector<TilePos> ends = next;
            for (auto rest = std::next(leg); rest != legs_list.end(); ++rest){
                ends = counted_ends(ends, *rest, policy);
            }
            return Pokemon::outcome_ok(recover_tapped_leg(from, at, ends, legs_list.back().direction, *leg, step, place.moved));
        }
        if (walk.moved_chunks + (one_short_ok ? 1 : 0) < tiles){
            return Pokemon::outcome_failed("short_leg", std::format("{} {} moved {}", direction_name(leg->direction), tiles, walk.moved_chunks));
        }
        at = next;
        m_legs.end_leg(traced_tiles(from.map, at), Pokemon::LegEndKnown::COUNTED);
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose(from.map, at, legs_list.back().direction));
}

//  A LEGS step "measured": each leg tapped, its taps that moved counted
//  against the map's from each tile the player may stand on; the tiles
//  whose count matches are where we are. A wall a tap bumps tells us
//  where we are, where a fixed leg from a guessed tile walks on astray
//  (00.03, lab-B 2026-11-09: a row low, the taps crossed the door's mat).
Outcome StoryWalkSteps::measured_legs(const Pose& from, const StoryStep& step){
    const StoryWorld* world = story_plan_world(from.map, from.map);
    std::vector<TilePos> at = Pokemon::tiles_of(from);
    if (world == nullptr || at.empty()){
        return Pokemon::outcome_failed("measured", "no map or tiles: " + Pokemon::to_string(from));
    }
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
    for (const Pokemon::StoryLeg& leg : legs_list){
        turn(leg.direction);
        const size_t tiles = (size_t)leg.tiles;
        m_legs.begin_leg({.direction = leg.direction, .tiles = tiles, .end_kind = Pokemon::LegEndKind::TILE});
        const WalkResult walk = m_route.walk_taps(leg.direction, tiles, step.name);
        if (walk.wild_battles > 0){
            return Pokemon::outcome_failed("interrupted_leg_unconfirmed", step.name);
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
            return Pokemon::outcome_failed("measured", std::format("{} moved {}: no tile of {} fits",
                direction_name(leg.direction), walk.moved_chunks, Pokemon::to_string(from)));
        }
        at = std::move(next);
        m_legs.end_leg(traced_tiles(from.map, at), Pokemon::LegEndKnown::COUNTED);
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose(from.map, at, legs_list.back().direction));
}

//  A LEGS step "counted_wall": its one leg held to the wall, its tiles
//  counted against the map's from each tile the table may have us on
//  (walk_counted_wall_leg()). The leg's tiles are the table's reading of
//  the map, and must be among them.
Outcome StoryWalkSteps::counted_wall(const Pose& from, const StoryStep& step){
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
    const StoryWorld* world = story_plan_world(from.map, from.map);
    const std::vector<TilePos> starts = Pokemon::tiles_of(from);
    if (legs_list.size() != 1 || world == nullptr || starts.empty()){
        return Pokemon::outcome_failed("counted_wall", std::format("{} legs from {}", legs_list.size(), Pokemon::to_string(from)));
    }
    const Pokemon::StoryLeg& leg = legs_list.front();
    size_t fewest = SIZE_MAX;
    size_t most = 0;
    for (const TilePos& pos : starts){
        const size_t tiles = slide_to_wall(*world, story_to_world(*world, from.map, pos), leg.direction).tiles;
        fewest = std::min(fewest, tiles);
        most = std::max(most, tiles);
    }
    if ((size_t)leg.tiles < fewest || (size_t)leg.tiles > most){
        return Pokemon::outcome_failed("map_mismatch",
            std::format("{} {} where the map has {} to {}", direction_name(leg.direction), leg.tiles, fewest, most));
    }
    turn(leg.direction);
    m_legs.begin_leg({.direction = leg.direction, .tiles = (size_t)leg.tiles, .end_kind = Pokemon::LegEndKind::WALL});
    walk_counted_wall_leg(m_route, {.direction = leg.direction, .fewest = fewest, .most = most,
        .speed = m_can_run ? MoveSpeed::RUN : MoveSpeed::WALK, .label = label_of(step)});
    m_legs.end_leg({}, Pokemon::LegEndKnown::WALL);
    return Pokemon::outcome_ok();
}

Outcome StoryWalkSteps::legs(RunState& state, const StoryStep& step){
    const EventTrace trace(m_legs, m_row, step.name);
    m_warp = {};
    m_scene_open = false;
    if (step.action == "counted_wall"){
        return counted_wall(state.pose, step);
    }
    if (step.action == "measured"){
        return measured_legs(state.pose, step);
    }
    if (step.action == "to_wall"){
        const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
        //  The walls end these legs; the step's pose_after says where.
        for (const Pokemon::StoryLeg& leg : legs_list){
            m_legs.begin_leg({.direction = leg.direction, .tiles = (size_t)leg.tiles, .end_kind = Pokemon::LegEndKind::WALL});
            m_io.walk_until_stopped(leg.direction, (size_t)leg.tiles, label_of(step));
            m_legs.end_leg({}, Pokemon::LegEndKnown::WALL);
        }
        return Pokemon::outcome_ok();
    }
    if (step.action == "to_map"){
        return leg_to_map(step);
    }
    if (step.action != "one_short_ok" && !step.action.empty() && step.action != "legs"){
        return Pokemon::outcome_failed("unknown_action", step.action);
    }
    return tap_legs(state.pose, step);
}

Outcome StoryWalkSteps::step(RunState& state, const StoryStep& step){
    const EventTrace trace(m_legs, m_row, step.name);
    const Direction direction = story_arg<Direction>(step.args, "direction");
    if (step.action == "to_banner"){
        turn(direction);
        return tap_to_banner(state.pose, step);
    }
    if (step.action == "tap"){
        //  No separate turn tap first: at 1 tile from the door, a turn tap
        //  steps onto the warp, causing walk_taps to take an extra step on the far map.
        const WalkResult walk = m_route.walk_taps(direction, 1, step.name);
        if (walk.wild_battles > 0){
            return Pokemon::outcome_failed("interrupted_step", step.name);
        }
        m_io.settle(BUTTON_B, label_of(step), false, "");
        return Pokemon::outcome_ok();
    }
    //  A step through a door: walk (or run) on until the screen goes dark,
    //  from next to the door or from anywhere in its line (r43: tapping 8 tiles
    //  down the lab took 7 s), then settle for any scene on arrival (Mom's,
    //  00.01; Dawn's tour, 00.04). Without the fade, settle must see it itself
    //  (r33 was outside, "no fade"). No separate turn tap first: at 1 tile
    //  from the door, a 50 ms turn tap can step in before run_into_door() watches.
    //  A fade or the far map's banner is the way through, either one (#268
    //  slice 1); with neither yet, settle() waits for one.
    if (step.pose_after && step.pose_after->map != state.pose.map){
        if (through_already(step)){
            m_io.settle(BUTTON_B, label_of(step), false, "");
            return Pokemon::outcome_ok();
        }
        const std::string banner = story_arrival_banner(state.pose.map, step.pose_after->map);
        const DoorRun door = door_run(state.pose.map, state.pose, direction, m_can_run);
        WarpSeen seen = m_io.run_into_door(direction, m_can_run, door.timeout, door.dark_from, banner);
        log(std::format("[{}] {}: {} through the door (timeout {} ms, dark from {} ms): {}.", m_row, step.name,
            direction_name(direction), door.timeout.count(), door.dark_from.count(), seen.any() ? describe(seen) : "nothing yet"));
        seen = door_from_beside(step, seen, door.timeout, banner);
        m_io.settle(BUTTON_B, label_of(step), !seen.any(), banner);
        return Pokemon::outcome_ok();
    }
    return one_step(state, step);
}

//  A plain D-pad step: onto a scene's trigger, mostly. Not taken when the
//  route before ran into that scene already (RouteEnd::scene): it holds us
//  where it started.
Outcome StoryWalkSteps::one_step(const RunState& state, const StoryStep& step){
    if (std::exchange(m_scene_open, false)){
        log(std::format("[{}] {}: its scene started on the walk before; no step.", m_row, step.name));
        return Pokemon::outcome_ok(state.pose);
    }
    const Direction direction = story_arg<Direction>(step.args, "direction");
    turn(direction);
    const WalkResult walk = m_route.walk_taps(direction, 1, step.name);
    if (walk.wild_battles > 0){
        return Pokemon::outcome_failed("interrupted_step", step.name);
    }
    log(std::format("[{}] {}: a step {}, {}.", m_row, step.name, direction_name(direction), walk_stop_name(walk.stop)));
    std::vector<TilePos> at;
    for (const TilePos& pos : Pokemon::tiles_of(state.pose)){
        at.push_back(pos.next(direction));
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose(state.pose.map, at, direction));
}

Outcome StoryWalkSteps::talk(RunState& state, const StoryStep& step){
    m_warp = {};
    m_scene_open = false;
    //  Face whom we talk to: a route ends facing its last leg, not its goal
    //  (r40 reached Mom facing north). They block the tile, so the tap only turns.
    if (const std::optional<Direction> facing = Pokemon::facing_of(state.pose)){
        turn(*facing);
    }
    m_route.press({.kind = RouteInputKind::BUTTON_A, .direction = Direction::NORTH, .hold = 80ms, .release = 200ms,
        .count = 1, .end = RouteInputEnd::DONE});
    //  Roark's first line in the mine comes after the camera zooms in on
    //  us, which can take some seconds (r98; the old event waited 8 s).
    if (!m_route.dialog_within(8000ms, COLOR_YELLOW)){
        fail(m_route, label_of(step) + ": nobody answered.");
    }
    return Pokemon::outcome_ok();
}

Outcome StoryWalkSteps::mash(RunState& state, const StoryStep& step){
    m_warp = {};
    m_scene_open = false;
    //  The step before may have played the scene already (a door step settles
    //  its arrival scene), so its first line is not required.
    if (!m_route.dialog_within(15000ms, COLOR_CYAN)){
        m_route.note(label_of(step) + ": no line within 15 s; the step before played it.", RouteNote::WARNING);
    }
    m_io.settle(step.action == "a" ? BUTTON_A : BUTTON_B, label_of(step),
        step.pose_after && step.pose_after->map != state.pose.map &&
        story_plan_world(state.pose.map, step.pose_after->map) == nullptr, "");
    if (m_row == RUNNING_SHOES_ROW && step.name == RUNNING_SHOES_STEP){
        m_can_run = true;
    }
    return Pokemon::outcome_ok();
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
