/*  BDSP AutoStory - Story executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Actions.h"
#include "PokemonBDSP_AutoStory_Center.h"
#include "PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP_AutoStory_Settings.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryVariants.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Tries.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

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


std::optional<RobustPlanCosts> story_route_costs(const std::string& variant){
    const std::optional<RouteCosts> costs = parse_variant<RouteCosts>(variant);
    if (!costs){
        return std::nullopt;
    }
    switch (*costs){
    case RouteCosts::TOWN:
        return town_plan_costs();
    case RouteCosts::ROBUST:
        return RobustPlanCosts();
    default:
        return route_plan_costs();
    }
}


namespace{

//  "to_catch_grass" as "to catch grass", for the logs.
std::string label_of(const StoryStep& step){
    std::string label = step.name;
    std::replace(label.begin(), label.end(), '_', ' ');
    return label;
}

//  The screen at a failed step, kept: the retry resets the game.
void save_failure_screenshot(VideoStream& stream, size_t attempt){
    VideoSnapshot screen = stream.video().snapshot();
    if (!screen){
        return;
    }
    const std::string folder = SCREENSHOTS_PATH() + "AutoStory/failures/";
    Filesystem::create_directories(Filesystem::Path(folder));
    const std::string path = std::format("{}{}-attempt{}.png", folder, now_to_filestring(), attempt);
    screen.frame->save(path);
    stream.log("Failure screenshot: " + path);
}


//  Walks `leg` until someone stops us and talks, for at most its tiles'
//  time; their dialogue must open.
void walk_until_someone_talks(VideoStream& stream, ProControllerContext& context, const Pokemon::StoryLeg& leg, const std::string& label){
    const auto [x, y] = dpad_of(leg.direction);
    walk_until(stream, context, run_from_wild_battle, {x, y}, leg.tiles * route_ms_per_tile(leg.direction) + 800ms, label);
    ShortDialogWatcher opened(COLOR_CYAN);
    if (wait_until(stream, context, 5000ms, {{opened}}) != 0){
        fail_step(stream, label + ": nobody talked to us.");
    }
}

//  Faces `direction`, then A until a dialogue opens, TALK_FACING_TRIES
//  times at most.
void talk_facing_tries(VideoStream& stream, ProControllerContext& context, Direction direction, const std::string& label){
    route_turn(stream, context, direction);
    const std::optional<size_t> answered = try_bounded(TALK_FACING_TRIES,
        [&](size_t){
            pbf_press_button(context, BUTTON_A, 80ms, 200ms);
            context.wait_for_all_requests();
            ShortDialogWatcher dialog(COLOR_YELLOW);
            return wait_until(stream, context, 3000ms, {{dialog}}) == 0;
        },
        [&](size_t attempt){
            stream.log(std::format("{}: nobody answered A {}; again.", label, attempt));
        }
    );
    if (!answered){
        fail_step(stream, std::format("{}: nobody answered {} A presses facing {}.", label, TALK_FACING_TRIES, direction_name(direction)));
    }
}

//  MASH_WAIT_OUT_PRESSES B presses 1.5 s apart, then up to as many more
//  until the overworld shows: fails only if it never does.
void wait_out_with_b(VideoStream& stream, ProControllerContext& context, const std::string& label){
    for (size_t press = 0; press < 2 * MASH_WAIT_OUT_PRESSES; press++){
        if (press >= MASH_WAIT_OUT_PRESSES){
            context.wait_for_all_requests();
            const VideoSnapshot screen = stream.video().snapshot();
            if (screen && OverworldDetector().detect(*screen.frame)){
                return;
            }
        }
        pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
    }
    context.wait_for_all_requests();
    const VideoSnapshot screen = stream.video().snapshot();
    if (!screen || !OverworldDetector().detect(*screen.frame)){
        fail_step(stream, label + ": no overworld after " + std::to_string(2 * MASH_WAIT_OUT_PRESSES) + " B presses.");
    }
}

//  One stick hold per tile of a hold leg: an east or west tile in 160 ms, a
//  north or south one in 200 ms, as the Jubilife walks were measured.
Milliseconds hold_per_tile(Direction direction){
    return direction == Direction::EAST || direction == Direction::WEST ? 160ms : 200ms;
}
void hold_leg(VideoStream& stream, ProControllerContext& context, const Pokemon::StoryLeg& leg, LegsWay way, const std::string& label){
    route_turn(stream, context, leg.direction);
    if (way == LegsWay::HOLD){
        hold_for(stream, context, leg.direction, leg.tiles * hold_per_tile(leg.direction), label);
        return;
    }
    for (int tile = 0; tile < leg.tiles; tile++){
        hold_for(stream, context, leg.direction, hold_per_tile(leg.direction), label);
    }
}

//  `step`'s legs in counted D-pad taps from `from`: where we may be after.
Outcome walk_dpad_legs(
    VideoStream& stream, ProControllerContext& context, const std::string& checkpoint,
    const Pose& from, const StoryStep& step, bool one_short_ok
){
    std::vector<TilePos> at = Pokemon::tiles_of(from);
    for (const Pokemon::StoryLeg& leg : step.legs){
        //  Facing another way, a tap only turns (r256): turn first.
        route_turn(stream, context, leg.direction);
        const auto [x, y] = dpad_of(leg.direction);
        const size_t tiles = (size_t)leg.tiles;
        const WalkResult walk = walk_tiles_dpad(stream, context, run_from_wild_battle, {x, y}, tiles, step.name);
        stream.log(std::format("[{}] {}: taps {} moved {} of {} tiles.", checkpoint, step.name, direction_name(leg.direction), walk.moved_chunks, tiles));
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

}


StoryExecutor::StoryExecutor(StoryEnv& env)
    : m_env(env)
{}

void StoryExecutor::begin_attempt(const Pokemon::StoryCheckpoint& data){
    m_checkpoint = data.id;
    const StoryWorld* south = story_plan_world("Route202", "Route202");
    m_trainers = south ? story_trainers(*south) : std::vector<TrainerSight>{};
}

Outcome StoryExecutor::run(RunState& state, const StoryStep& step){
    stream().log(std::format("[{}] {} {}: {}", m_checkpoint, Pokemon::story_step_kind_name(step.kind), step.name, step.why), COLOR_BLUE);
    try{
        return run_guarded(state, step);
    }catch (OperationFailedException& e){
        if (m_stop_rows.contains(m_checkpoint)){
            throw;
        }
        //  Attempts are the segment's policy; this only counts them for the
        //  log line the lab's run watcher reads.
        const size_t attempt = m_failures[m_checkpoint]++;
        stream().log(std::format("checkpoint_reattempt_loop: attempt {} failed: {}: {}", attempt, m_checkpoint, e.message()), COLOR_RED);
        save_failure_screenshot(stream(), attempt);
        return Pokemon::outcome_failed("operation_failed", e.message());
    }
}

Outcome StoryExecutor::run_guarded(RunState& state, const StoryStep& step){
    const StoryEvents& events = story_events();
    if (auto event = events.find(step.variant); event != events.end()){
        return event->second(*this, state, step);
    }
    return Pokemon::StoryStepExecutor::run(state, step);
}

Pose StoryExecutor::walk_to(const Pose& from, const Pose& goal, const StoryStep& step){
    const StoryWorld* maps = story_plan_world(from.map, goal.map);
    if (maps == nullptr){
        fail_step(stream(), std::format("{}: no maps join {} and {}.", step.name, from.map, goal.map));
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
    //  Only Route 202's trainers are known; the ones fought stay off
    //  m_trainers until the next attempt.
    const bool south = maps == story_plan_world("Route202", "Route202");
    RouteTrainers trainers{
        south ? m_trainers : std::vector<TrainerSight>{},
        [this](const std::string& trainer){ fight_route_trainer(stream(), context(), trainer); }
    };
    const RouteWalkPlan plan{
        [&](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); },
        label_of(step), {}, *costs
    };
    const std::vector<TilePos> at = walk_route(stream(), context(), run_from_wild_battle, *maps, start, trainers, plan);
    if (south){
        m_trainers = std::move(trainers.sights);
    }
    std::vector<TilePos> local;
    for (const TilePos& pos : at){
        const std::optional<TilePos> tile = story_to_local(*maps, goal.map, pos);
        if (!tile){
            fail_step(stream(), std::format("{}: the walk ended off {}.", step.name, goal.map));
        }
        local.push_back(*tile);
    }
    return Pokemon::tile_pose(goal.map, local, Pokemon::facing_of(goal));
}

Outcome StoryExecutor::new_game(RunState&, const StoryStep&){
    play_new_game_intro(m_env.env, context(), m_env.player_name);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::options(RunState&, const StoryStep&){
    verify_tutorial_settings(stream(), context());
    context().wait_for_all_requests();
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::route(RunState& state, const StoryStep& step){
    if (Pokemon::tiles_of(state.pose).empty()){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    return Pokemon::outcome_ok(walk_to(state.pose, *step.target, step));
}

Outcome StoryExecutor::legs(RunState& state, const StoryStep& step){
    const std::optional<LegsWay> way = parse_variant<LegsWay>(step.variant);
    if (!way){
        return Pokemon::outcome_failed("unknown_variant", step.variant);
    }
    switch (*way){
    case LegsWay::TO_WALL:
        for (const Pokemon::StoryLeg& leg : step.legs){
            walk_until_stopped(stream(), context(), leg.direction, (size_t)leg.tiles, label_of(step));
        }
        return Pokemon::outcome_ok();
    case LegsWay::HOLD:
    case LegsWay::HOLD_TAPS:
        for (const Pokemon::StoryLeg& leg : step.legs){
            hold_leg(stream(), context(), leg, *way, label_of(step));
        }
        return Pokemon::outcome_ok();
    default:
        return walk_dpad_legs(stream(), context(), m_checkpoint, state.pose, step, *way == LegsWay::DPAD_ONE_SHORT_OK);
    }
}

Outcome StoryExecutor::step(RunState& state, const StoryStep& step){
    const std::optional<StepWay> way = parse_variant<StepWay>(step.variant);
    if (!way){
        return Pokemon::outcome_failed("unknown_variant", step.variant);
    }
    if (*way == StepWay::OUT_SOUTH){
        leave_door_south(stream(), context(), run_from_wild_battle, label_of(step));
        return Pokemon::outcome_ok();
    }
    route_turn(stream(), context(), step.direction);
    const auto [x, y] = dpad_of(step.direction);
    const WalkResult walk = walk_tiles_dpad(stream(), context(), run_from_wild_battle, {x, y}, 1, step.name);
    stream().log(std::format("[{}] {}: a step {}, {}.", m_checkpoint, step.name, direction_name(step.direction), walk_stop_name(walk.stop)));
    //  A step onto a door changes the map, and the fade can start after the
    //  walk's window (r10: Barry's door). Wait it out here, so the next step
    //  doesn't take it for its own stop.
    if (walk.stop != WalkStop::TRANSITION && step.pose_after && step.pose_after->map != state.pose.map){
        BlackScreenOverWatcher faded(COLOR_RED);
        if (wait_until(stream(), context(), 5000ms, {{faded}}) != 0){
            fail_step(stream(), label_of(step) + ": the map didn't change.");
        }
    }
    std::vector<TilePos> at;
    for (const TilePos& pos : Pokemon::tiles_of(state.pose)){
        at.push_back(pos.next(step.direction));
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose(state.pose.map, at, step.direction));
}

Outcome StoryExecutor::talk(RunState&, const StoryStep& step){
    const std::optional<TalkWay> way = parse_variant<TalkWay>(step.variant);
    if (!way){
        return Pokemon::outcome_failed("unknown_variant", step.variant);
    }
    if (*way == TalkWay::WALKING){
        walk_until_someone_talks(stream(), context(), step.legs.front(), label_of(step));
        return Pokemon::outcome_ok();
    }
    if (*way == TalkWay::FACING){
        talk_facing_tries(stream(), context(), step.direction, label_of(step));
        return Pokemon::outcome_ok();
    }
    pbf_press_button(context(), BUTTON_A, 80ms, 200ms);
    context().wait_for_all_requests();
    ShortDialogWatcher dialog(COLOR_YELLOW);
    if (wait_until(stream(), context(), 3000ms, {{dialog}}) != 0){
        fail_step(stream(), label_of(step) + ": nobody answered.");
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::answer(RunState&, const StoryStep& step){
    wait_for_dialogue(stream(), context(), label_of(step));
    pbf_press_button(context(), step.yes ? BUTTON_A : BUTTON_B, 80ms, 1000ms);
    context().wait_for_all_requests();
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::mash(RunState&, const StoryStep& step){
    const std::optional<MashWay> way = parse_variant<MashWay>(step.variant);
    if (!way){
        return Pokemon::outcome_failed("unknown_variant", step.variant);
    }
    if (*way == MashWay::WAIT_OUT){
        wait_out_with_b(stream(), context(), label_of(step));
        return Pokemon::outcome_ok();
    }
    wait_for_dialogue(stream(), context(), label_of(step));
    finish_dialogue(stream(), context(), *way == MashWay::A ? BUTTON_A : BUTTON_B, label_of(step));
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::battle(RunState&, const StoryStep& step){
    const std::optional<BattleFormat> format = parse_variant<BattleFormat>(step.variant);
    if (!format){
        return Pokemon::outcome_failed("unknown_variant", step.variant);
    }
    if (!talk_or_fight(stream(), context(), label_of(step), TalkExpect::BATTLE, *format)){
        fail_step(stream(), label_of(step) + ": talked but did not battle.");
    }
    if (!step.may_lose && in_pokemon_center(stream())){
        fail_step(stream(), label_of(step) + ": lost the battle.");
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::train(RunState&, const StoryStep& step){
    for (size_t battle = 0; battle < step.battles; battle++){
        const std::string label = std::format("Training battle {}/{}", battle + 1, step.battles);
        route_status(stream(), label);
        battle_1_pokemon(stream(), context());
        fail_if_blacked_out(stream(), context(), label);
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::heal(RunState&, const StoryStep& step){
    heal_at_pokemon_center(stream(), context(), label_of(step));
    //  r143a's heal said it had walked out while still inside.
    context().wait_for_all_requests();
    VideoSnapshot screen = stream().video().snapshot();
    if (screen && indoors(*screen.frame)){
        fail_step(stream(), label_of(step) + ": still inside the Pokemon Center.");
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::save(RunState&, const StoryStep&){
    save_game(m_env.env, context());
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::game(RunState&, const StoryStep& step){
    return Pokemon::outcome_failed("unknown_event", step.variant);
}


Pokemon::Segment make_story_segment(
    const std::vector<StoryCheckpoint>& rows, Pokemon::StoryStepExecutor& executor,
    std::set<std::string>& saved
){
    static const StoryStep SAVE_AT_START = []{
        StoryStep step;
        step.kind = Pokemon::StoryStepKind::SAVE;
        step.name = "save";
        step.why = "the save a failed step resets to";
        return step;
    }();
    Pokemon::Segment segment;
    segment.id = "BDSP";
    for (const StoryCheckpoint& row : rows){
        segment.checkpoints.push_back(Pokemon::CheckpointSpec{
            .id = row.id, .title = row.title, .saves = false,
            .build = [&row, &executor, &saved]{
                Pokemon::Checkpoint steps = Pokemon::build_story_checkpoint(row, executor);
                if (row.on_failure != StoryFailure::RELOAD_AND_RETRY){
                    return steps;
                }
                Pokemon::Event save = Pokemon::story_event(SAVE_AT_START, executor);
                save.skip_if.push_back(Pokemon::condition("saved at the start", [&saved, id = row.id](RunState&){
                    return saved.contains(id);
                }));
                //  A save that fails is a failed first attempt: the retry
                //  resets and does not save again.
                save.act = [&executor, &saved, id = row.id](RunState& state){
                    saved.insert(id);
                    return executor.run(state, SAVE_AT_START);
                };
                Pokemon::Checkpoint checkpoint;
                checkpoint.id = row.id;
                checkpoint.events.push_back(std::move(save));
                for (Pokemon::Event& event : steps.events){
                    checkpoint.events.push_back(std::move(event));
                }
                return checkpoint;
            },
        });
    }
    return segment;
}



}
}
}
