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
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Navigation/Pokemon_TilePose.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Actions.h"
#include "PokemonBDSP_AutoStory_Center.h"
#include "PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP_AutoStory_Settings.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_Talks.h"
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
    if (variant.empty()){
        return route_plan_costs();
    }
    if (variant == "town"){
        return town_plan_costs();
    }
    if (variant == "robust"){
        return RobustPlanCosts();
    }
    return std::nullopt;
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
    std::vector<TrainerSight> no_trainers;
    const bool south = maps == story_plan_world("Route202", "Route202");
    const std::vector<TilePos> at = walk_route(
        stream(), context(), run_from_wild_battle, *maps, start,
        [&](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); },
        south ? m_trainers : no_trainers,
        [this](const std::string& trainer){ fight_route_trainer(stream(), context(), trainer); },
        label_of(step), {}, *costs
    );
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
    if (step.variant == "to_wall"){
        for (const Pokemon::StoryLeg& leg : step.legs){
            walk_until_stopped(stream(), context(), leg.direction, (size_t)leg.tiles, label_of(step));
        }
        return Pokemon::outcome_ok();
    }
    const bool one_short_ok = step.variant == "one_short_ok";
    if (!one_short_ok && !step.variant.empty()){
        return Pokemon::outcome_failed("unknown_variant", step.variant);
    }
    std::vector<TilePos> at = Pokemon::tiles_of(state.pose);
    for (const Pokemon::StoryLeg& leg : step.legs){
        //  Facing another way, a tap only turns (r256): turn first.
        route_turn(stream(), context(), leg.direction);
        const auto [x, y] = dpad_of(leg.direction);
        const size_t tiles = (size_t)leg.tiles;
        const WalkResult walk = walk_tiles_dpad(stream(), context(), run_from_wild_battle, x, y, tiles, step.name);
        stream().log(std::format("[{}] {}: taps {} moved {} of {} tiles.", m_checkpoint, step.name, direction_name(leg.direction), walk.moved_chunks, tiles));
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
    return Pokemon::outcome_ok(Pokemon::tile_pose(state.pose.map, at, step.legs.back().direction));
}

Outcome StoryExecutor::step(RunState& state, const StoryStep& step){
    route_turn(stream(), context(), step.direction);
    const auto [x, y] = dpad_of(step.direction);
    const WalkResult walk = walk_tiles_dpad(stream(), context(), run_from_wild_battle, x, y, 1, step.name);
    stream().log(std::format("[{}] {}: a step {}, {}.", m_checkpoint, step.name, direction_name(step.direction), walk_stop_name(walk.stop)));
    std::vector<TilePos> at;
    for (const TilePos& pos : Pokemon::tiles_of(state.pose)){
        at.push_back(pos.next(step.direction));
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose(state.pose.map, at, step.direction));
}

Outcome StoryExecutor::talk(RunState&, const StoryStep& step){
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
    wait_for_dialogue(stream(), context(), label_of(step));
    finish_dialogue(stream(), context(), step.variant == "a" ? BUTTON_A : BUTTON_B, label_of(step));
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::battle(RunState&, const StoryStep& step){
    if (!talk_or_fight(stream(), context(), label_of(step), true, step.variant == "doubles")){
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
