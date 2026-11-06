/*  BDSP AutoStory - Story walk steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::TilePos;


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

} // namespace


void StoryWalkSteps::begin_attempt(const std::string& row){
    m_row = row;
    //  A row starts from its save: after the shoes' row, the save has them.
    m_can_run = row > std::string(RUNNING_SHOES_ROW);
    m_trainers.clear();
}

void StoryWalkSteps::turn(Direction direction){
    m_io.route().note(std::string("[Route] Turning ") + direction_name(direction) + ".", RouteNote::LOG);
    m_io.route().press(route_turn_tap(direction));
}

void StoryWalkSteps::log(const std::string& text){
    m_io.route().note(text, RouteNote::LOG);
}

Pose StoryWalkSteps::walk_to(const Pose& from, const Pose& goal, const StoryStep& step){
    const StoryWorld* maps = story_plan_world(from.map, goal.map);
    if (maps == nullptr){
        fail(m_io.route(), std::format("{}: no maps join {} and {}.", step.name, from.map, goal.map));
    }
    const std::optional<RobustPlanCosts> costs = story_route_costs(step.variant);
    if (!costs){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown route costs: " + step.variant);
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
    std::vector<TrainerSight>& sights = m_trainers.try_emplace(maps, story_trainers(*maps)).first->second;
    RouteTrainers trainers{
        sights,
        [this](const std::string& trainer){ m_io.fight(trainer); }
    };
    const RouteWalkPlan plan{
        [&](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); },
        label_of(step), {}, *costs, m_can_run ? MoveSpeed::RUN : MoveSpeed::WALK
    };
    const std::vector<TilePos> at = walk_route(m_io.route(), *maps, start, trainers, plan);
    sights = std::move(trainers.sights);
    std::vector<TilePos> local;
    for (const TilePos& pos : at){
        const std::optional<TilePos> tile = story_to_local(*maps, goal.map, pos);
        if (!tile){
            fail(m_io.route(), std::format("{}: the walk ended off {}.", step.name, goal.map));
        }
        local.push_back(*tile);
    }
    return Pokemon::tile_pose(goal.map, local, Pokemon::facing_of(goal));
}

Outcome StoryWalkSteps::route(RunState& state, const StoryStep& step){
    if (Pokemon::tiles_of(state.pose).empty()){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    return Pokemon::outcome_ok(walk_to(state.pose, *step.target, step));
}

//  A LEGS step "to_dialogue": taps its one leg until a dialogue starts on the
//  way, wherever it starts (Dawn's at Sandgem's edge, r35/r36: an exact tile
//  and banner raced her scene). The scene's lines are the next step's.
Outcome StoryWalkSteps::leg_to_dialogue(const StoryStep& step){
    const Pokemon::StoryLeg& leg = step.legs.front();
    turn(leg.direction);
    const WalkResult walk = m_io.route().walk_taps(leg.direction, (size_t)leg.tiles, step.name);
    log(std::format("[{}] {}: taps {} to a dialogue: {} tiles, {}.", m_row, step.name, direction_name(leg.direction), walk.moved_chunks, walk_stop_name(walk.stop)));
    if (walk.stop == WalkStop::DIALOG){
        return Pokemon::outcome_ok();
    }
    //  A scene can lock us before its first line: Dawn walks over first (r37).
    if (walk.stop == WalkStop::BLOCKED && m_io.route().dialog_within(10000ms, COLOR_CYAN)){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_failed("no_dialogue", step.name);
}

//  A LEGS step "to_map": runs its one leg until the location banner of
//  pose_after's map shows, wherever it shows. A straight run along an open
//  row needs no tile count (r41, r42: Sandgem's row 11 took us into the lab).
Outcome StoryWalkSteps::leg_to_map(const StoryStep& step){
    const Pokemon::StoryLeg& leg = step.legs.front();
    const std::string map = step.pose_after ? step.pose_after->map : "";
    turn(leg.direction);
    const bool arrived = m_io.run_until_map(leg.direction, Milliseconds(250) * leg.tiles + 2000ms, map, step.name);
    return arrived ? Pokemon::outcome_ok() : Pokemon::outcome_failed("no_banner", map);
}

//  A STEP "to_banner": D-pad taps toward a door or cave mouth until the fade
//  and pose_after's map's location banner. No banner fails the step right
//  there.
Outcome StoryWalkSteps::tap_to_banner(const StoryStep& step){
    const std::string map = step.pose_after ? step.pose_after->map : "";
    const std::optional<size_t> taps = m_io.tap_to_banner(step.direction, map, 30);
    if (!taps){
        return Pokemon::outcome_failed("no_banner", map);
    }
    log(std::format("[{}] {}: {}'s banner after {} taps {}.", m_row, step.name, map, *taps, direction_name(step.direction)));
    return Pokemon::outcome_ok();
}

//  Each leg in D-pad taps, turning first; "one_short_ok" lets a leg end a
//  tile short.
Outcome StoryWalkSteps::tap_legs(const Pose& from, const StoryStep& step){
    const bool one_short_ok = step.variant == "one_short_ok";
    std::vector<TilePos> at = Pokemon::tiles_of(from);
    for (const Pokemon::StoryLeg& leg : step.legs){
        //  Facing another way, a tap only turns (r256): turn first.
        turn(leg.direction);
        const size_t tiles = (size_t)leg.tiles;
        const WalkResult walk = m_io.route().walk_taps(leg.direction, tiles, step.name);
        log(std::format("[{}] {}: taps {} moved {} of {} tiles.", m_row, step.name, direction_name(leg.direction), walk.moved_chunks, tiles));
        if (walk.moved_chunks + (one_short_ok ? 1 : 0) < tiles){
            return Pokemon::outcome_failed("short_leg", std::format("{} {} moved {}", direction_name(leg.direction), tiles, walk.moved_chunks));
        }
        //  Where we may be: the end, or a tile short of it.
        std::vector<TilePos> next;
        for (const TilePos& start : at){
            TilePos end = start;
            for (size_t i = 0; i < tiles; i++){
                if (one_short_ok && i + 1 == tiles){
                    next.push_back(end);
                }
                end = end.next(leg.direction);
            }
            next.push_back(end);
        }
        at = std::move(next);
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose(from.map, at, step.legs.back().direction));
}

Outcome StoryWalkSteps::legs(RunState& state, const StoryStep& step){
    if (step.variant == "to_wall"){
        for (const Pokemon::StoryLeg& leg : step.legs){
            m_io.walk_until_stopped(leg.direction, (size_t)leg.tiles, label_of(step));
        }
        return Pokemon::outcome_ok();
    }
    if (step.variant == "to_dialogue"){
        return leg_to_dialogue(step);
    }
    if (step.variant == "to_map"){
        return leg_to_map(step);
    }
    if (step.variant != "one_short_ok" && !step.variant.empty()){
        return Pokemon::outcome_failed("unknown_variant", step.variant);
    }
    return tap_legs(state.pose, step);
}

Outcome StoryWalkSteps::step(RunState& state, const StoryStep& step){
    turn(step.direction);
    if (step.variant == "to_banner"){
        return tap_to_banner(step);
    }
    //  A step through a door: walk (or run) on until the screen goes dark,
    //  from next to the door or from anywhere in its line (r43: tapping 8 tiles
    //  down the lab took 7 s), then settle for any scene on arrival (Mom's,
    //  00.01; Dawn's tour, 00.04). Without the fade, settle must see it itself
    //  (r33 was outside, "no fade").
    if (step.pose_after && step.pose_after->map != state.pose.map){
        const bool faded = m_io.run_into_door(step.direction, m_can_run);
        log(std::format("[{}] {}: {} through the door: {}.", m_row, step.name, direction_name(step.direction), faded ? "faded" : "no fade yet"));
        m_io.settle(BUTTON_B, label_of(step), !faded);
        return Pokemon::outcome_ok();
    }
    const WalkResult walk = m_io.route().walk_taps(step.direction, 1, step.name);
    log(std::format("[{}] {}: a step {}, {}.", m_row, step.name, direction_name(step.direction), walk_stop_name(walk.stop)));
    std::vector<TilePos> at;
    for (const TilePos& pos : Pokemon::tiles_of(state.pose)){
        at.push_back(pos.next(step.direction));
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose(state.pose.map, at, step.direction));
}

Outcome StoryWalkSteps::talk(RunState& state, const StoryStep& step){
    //  Face whom we talk to: a route ends facing its last leg, not its goal
    //  (r40 reached Mom facing north). They block the tile, so the tap only turns.
    if (const std::optional<Direction> facing = Pokemon::facing_of(state.pose)){
        turn(*facing);
    }
    m_io.route().press({.kind = RouteInputKind::BUTTON_A, .direction = Direction::NORTH, .hold = 80ms, .release = 200ms,
        .count = 1, .end = RouteInputEnd::DONE});
    //  Roark's first line in the mine comes after the camera zooms in on
    //  us, which can take some seconds (r98; the old event waited 8 s).
    if (!m_io.route().dialog_within(8000ms, COLOR_YELLOW)){
        fail(m_io.route(), label_of(step) + ": nobody answered.");
    }
    return Pokemon::outcome_ok();
}

Outcome StoryWalkSteps::mash(RunState& state, const StoryStep& step){
    //  The step before may have played the scene already (a door step settles
    //  its arrival scene), so its first line is not required.
    if (!m_io.route().dialog_within(15000ms, COLOR_CYAN)){
        m_io.route().note(label_of(step) + ": no line within 15 s; the step before played it.", RouteNote::WARNING);
    }
    m_io.settle(step.variant == "a" ? BUTTON_A : BUTTON_B, label_of(step),
        step.pose_after && step.pose_after->map != state.pose.map);
    if (m_row == RUNNING_SHOES_ROW && step.name == RUNNING_SHOES_STEP){
        m_can_run = true;
    }
    return Pokemon::outcome_ok();
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
