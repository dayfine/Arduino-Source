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
#include "CommonFramework/Exceptions/FatalProgramException.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_TrainerSight.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "Actions/PokemonBDSP_AutoStory_Actions.h"
#include "Actions/PokemonBDSP_AutoStory_Center.h"
#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "Actions/PokemonBDSP_AutoStory_NewGame.h"
#include "Actions/PokemonBDSP_AutoStory_Settings.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkDpad.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkScreen.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "Actions/PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "Actions/PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::TilePos;
using Pokemon::story_arg;


std::optional<RobustPlanCosts> story_route_costs(const std::string& action){
    if (action.empty() || action == "route"){
        return route_plan_costs();
    }
    if (action == "town"){
        return town_plan_costs();
    }
    if (action == "robust"){
        return RobustPlanCosts();
    }
    if (action == "line"){
        return waypoint_plan_costs();
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

//  A trainer who saw us on a route: Route 204's Twins battle as a pair
//  (01.10), everyone else alone.
void fight_story_trainer(VideoStream& stream, ProControllerContext& context, const std::string& trainer){
    if (trainer.find("Twins") != std::string::npos){
        talk_or_fight(stream, context, trainer, TalkExpect::BATTLE, BattleFormat::DOUBLES);
        return;
    }
    fight_route_trainer(stream, context, trainer);
}

} // namespace


StoryExecutor::StoryExecutor(StoryEnv& env)
    : m_env(env)
{}

void StoryExecutor::begin_attempt(const Pokemon::StoryCheckpoint& data){
    m_checkpoint = data.id;
    //  A row starts from its save: after the shoes' row, the save has them.
    m_can_run = data.id > std::string(RUNNING_SHOES_ROW);
    m_trainers.clear();
}

Outcome StoryExecutor::run(const StoryStep& step, RunState& state){
    stream().log(std::format("[{}] {} {}: {}", m_checkpoint, Pokemon::story_step_kind_name(step.kind), step.name, step.why), COLOR_BLUE);
    try{
        Outcome outcome = run_guarded(state, step);
        if (outcome.succeeded() && m_checkpoint == RUNNING_SHOES_ROW && step.name == RUNNING_SHOES_STEP){
            m_can_run = true;
        }
        return outcome;
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
    if (auto event = events.find(step.action); event != events.end()){
        return event->second(*this, state, step);
    }
    switch (step.kind){
    case Pokemon::StoryStepKind::NEW_GAME:
        return new_game(state, step);
    case Pokemon::StoryStepKind::OPTIONS:
        return options(state, step);
    case Pokemon::StoryStepKind::ROUTE:
        return route(state, step);
    case Pokemon::StoryStepKind::LEGS:
        return legs(state, step);
    case Pokemon::StoryStepKind::STEP:
        return this->step(state, step);
    case Pokemon::StoryStepKind::TALK:
        return talk(state, step);
    case Pokemon::StoryStepKind::ANSWER:
        return answer(state, step);
    case Pokemon::StoryStepKind::MASH:
        return mash(state, step);
    case Pokemon::StoryStepKind::BATTLE:
        return battle(state, step);
    case Pokemon::StoryStepKind::TRAIN:
        return train(state, step);
    case Pokemon::StoryStepKind::HEAL:
        return heal(state, step);
    case Pokemon::StoryStepKind::SAVE:
        return save(state, step);
    case Pokemon::StoryStepKind::GAME:
        return game(state, step);
    default:
        return Pokemon::unsupported_step(step);
    }
}

Pose StoryExecutor::walk_to(const Pose& from, const Pose& goal, const StoryStep& step){
    const StoryWorld* maps = story_plan_world(from.map, goal.map);
    if (maps == nullptr){
        fail_step(stream(), std::format("{}: no maps join {} and {}.", step.name, from.map, goal.map));
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
    std::vector<TrainerSight>& sights = m_trainers.try_emplace(maps, story_trainers(*maps)).first->second;
    RouteTrainers trainers{
        sights,
        [this](const std::string& trainer){ fight_story_trainer(stream(), context(), trainer); }
    };
    const RouteWalkPlan plan{
        [&](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); },
        label_of(step), {}, *costs, m_can_run ? MoveSpeed::RUN : MoveSpeed::WALK, pause_points()
    };
    const std::vector<TilePos> at = walk_route(stream(), context(), run_from_wild_battle, *maps, start, trainers, plan);
    sights = std::move(trainers.sights);
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
    const GridPose& target = story_arg<GridPose>(step.args, "target");
    return Pokemon::outcome_ok(walk_to(state.pose, Pokemon::pose_of(target), step));
}

namespace{

//  The breakpoint before leg `index` of `count` of a LEGS step, when `at` is
//  the one tile the leg starts on, to the tile it ends on.
void pause_before_leg(
    Pokemon::StoryPausePoints* pause, size_t index, size_t count, const Pokemon::StoryLeg& leg, const std::vector<TilePos>& at
){
    if (pause == nullptr){
        return;
    }
    std::optional<TilePos> end;
    if (at.size() == 1){
        end = at[0];
        for (int c = 0; c < leg.tiles; c++){
            end = end->next(leg.direction);
        }
    }
    pause->before_leg(Pokemon::story_leg_pause_label(index, count, leg.direction, leg.tiles, end));
}

//  A LEGS step "to_dialogue": taps its one leg until a dialogue starts on the
//  way, wherever it starts (Dawn's at Sandgem's edge, r35/r36: an exact tile
//  and banner raced her scene). The scene's lines are the next step's.
Outcome leg_to_dialogue(
    VideoStream& stream, ProControllerContext& context, const StoryStep& step, const std::string& checkpoint, Pokemon::StoryPausePoints* pause
){
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
    const Pokemon::StoryLeg& leg = legs_list.front();
    pause_before_leg(pause, 0, 1, leg, {});
    route_turn(stream, context, leg.direction);
    const auto [x, y] = dpad_of(leg.direction);
    const WalkResult walk = walk_tiles_dpad(stream, context, run_from_wild_battle, {x, y}, (size_t)leg.tiles, step.name);
    stream.log(std::format("[{}] {}: taps {} to a dialogue: {} tiles, {}.", checkpoint, step.name, direction_name(leg.direction), walk.moved_chunks, walk_stop_name(walk.stop)));
    if (walk.stop == WalkStop::DIALOG){
        return Pokemon::outcome_ok();
    }
    //  A scene can lock us before its first line: Dawn walks over first (r37).
    ShortDialogWatcher dialog(COLOR_CYAN);
    if (walk.stop == WalkStop::BLOCKED && wait_until(stream, context, 10000ms, {{dialog}}) == 0){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_failed("no_dialogue", step.name);
}

//  A LEGS step "to_map": runs its one leg until the location banner of
//  pose_after's map shows, wherever it shows. A straight run along an open
//  row needs no tile count (r41, r42: Sandgem's row 11 took us into the lab).
Outcome leg_to_map(
    VideoStream& stream, ProControllerContext& context, const StoryStep& step, const std::string& checkpoint, Pokemon::StoryPausePoints* pause
){
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
    const Pokemon::StoryLeg& leg = legs_list.front();
    pause_before_leg(pause, 0, 1, leg, {});
    const std::string map = step.pose_after ? Pokemon::grid_pose_of(*step.pose_after).map : "";
    route_turn(stream, context, leg.direction);
    const auto [x, y] = dpad_of(leg.direction);
    const WalkResult walk = walk_until_dpad(stream, context, run_from_wild_battle, {x, y},
        Milliseconds(250) * leg.tiles + 2000ms, step.name, {
            .arrived = [&](const ImageViewRGB32& screen){ return location_banner_matches(read_location_banner(screen), map); },
            .pace = DpadPace::RUN,
        });
    stream.log(std::format("[{}] {}: ran {} to {}'s banner: {}.", checkpoint, step.name, direction_name(leg.direction), map, walk_stop_name(walk.stop)));
    return walk.stop == WalkStop::ARRIVED ? Pokemon::outcome_ok() : Pokemon::outcome_failed("no_banner", map);
}

} // namespace

Outcome StoryExecutor::legs(RunState& state, const StoryStep& step){
    const auto& legs_list = story_arg<std::vector<Pokemon::StoryLeg>>(step.args, "legs");
    if (step.action == "to_wall"){
        for (size_t index = 0; index < legs_list.size(); index++){
            const Pokemon::StoryLeg& leg = legs_list[index];
            pause_before_leg(pause_points(), index, legs_list.size(), leg, {});
            walk_until_stopped(stream(), context(), leg.direction, (size_t)leg.tiles, label_of(step));
        }
        return Pokemon::outcome_ok();
    }
    if (step.action == "to_dialogue"){
        return leg_to_dialogue(stream(), context(), step, m_checkpoint, pause_points());
    }
    if (step.action == "to_map"){
        return leg_to_map(stream(), context(), step, m_checkpoint, pause_points());
    }
    const bool one_short_ok = step.action == "one_short_ok";
    if (!one_short_ok && !step.action.empty() && step.action != "legs"){
        return Pokemon::outcome_failed("unknown_action", step.action);
    }
    std::vector<TilePos> at = Pokemon::tiles_of(state.pose);
    for (size_t index = 0; index < legs_list.size(); index++){
        const Pokemon::StoryLeg& leg = legs_list[index];
        pause_before_leg(pause_points(), index, legs_list.size(), leg, at);
        //  Facing another way, a tap only turns (r256): turn first.
        route_turn(stream(), context(), leg.direction);
        const auto [x, y] = dpad_of(leg.direction);
        const size_t tiles = (size_t)leg.tiles;
        const WalkResult walk = walk_tiles_dpad(stream(), context(), run_from_wild_battle, {x, y}, tiles, step.name);
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
    return Pokemon::outcome_ok(Pokemon::tile_pose(state.pose.map, at, legs_list.back().direction));
}

namespace{

//  One rule for scenes and map changes: v2's long B mashes, with an end
//  check. Press through the text, and keep pressing B while the screen is
//  quiet: a scene can pause while someone walks off (Dawn and Barry after
//  Rowan leaves, r23), fade dimly, not to black, to another map (r25), or
//  wait on a box the detector misses (r26). B never starts a talk on the
//  overworld. Done once the overworld has shown, with no text and a still
//  screen, for 5 s of presses: after r31's fade home a scripted walk-in ran
//  5 s with no text before Mom spoke.
//
//  A step or scene that ends on another map (`fade`) must fade on the way,
//  and only the quiet after the fade counts: r30's scene went quiet for 5 s
//  before the fade home, and r28 "went out" a tile beside the door and
//  walked a whole route in the house. No fade is a map mismatch.
constexpr double FADE_DARKNESS = 40;    //  brightness 0 to 255; r24's dim fade home goes darker
void settle(VideoStream& stream, ProControllerContext& context, Button button, const std::string& label, bool fade){
    const WallClock none = WallClock::max();
    const WallClock deadline = current_time() + 120s;
    ShortDialogDetector dialog;
    OverworldDetector overworld;
    WallClock quiet_since = none;
    WallClock calm_since = none;    //  overworld, no text, still or not
    double darkest = 255;
    VideoSnapshot previous;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        const WallClock now = current_time();
        if (frame){
            darkest = std::min(darkest, image_average(*frame.frame).sum() / 3);
        }
        const bool talking = frame && dialog.detect(frame);
        const bool still = frame && previous && walk_screen_change(*previous.frame, *frame.frame) < WALK_TAP_BLOCKED_CHANGE;
        previous = frame;
        //  Cloud shadows drift over Floaroma, so its screen is never still: r83
        //  got Mew and then waited out the 120 s on a calm overworld. 15 s of
        //  overworld with no text is three times r31's silent walk-in.
        const bool calm = !talking && overworld.detect(frame) && (!fade || darkest < FADE_DARKNESS);
        calm_since = calm ? (calm_since == none ? now : calm_since) : none;
        if (calm && now - calm_since >= 15000ms){
            stream.log(std::format("[Story] {}: settled on 15 s of overworld without text (the screen kept moving); darkest frame {:.0f}.", label, darkest));
            return;
        }
        if (!talking && still && overworld.detect(frame) && (!fade || darkest < FADE_DARKNESS)){
            quiet_since = quiet_since == none ? now : quiet_since;
            if (now - quiet_since >= 5000ms){
                stream.log(std::format("[Story] {}: settled; darkest frame {:.0f}.", label, darkest));
                return;
            }
        }else{
            quiet_since = none;
        }
        pbf_press_button(context, talking ? button : BUTTON_B, 80ms, 1200ms);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        fade && darkest >= FADE_DARKNESS
            ? label + ": no fade within 120 s, so we are not where the step goes (map mismatch)."
            : label + ": not back on a quiet overworld within 120 s.",
        stream);
}

} // namespace

namespace{

//  Holds the D-pad toward a door (with B after the shoes) until the screen goes
//  dark or a line shows, up to 12 s. Returns whether it went dark. A line
//  stops it: holding on under Mom's "Oh, Kosmos!" wasted 12 s (r45). The walker's black-screen watch
//  missed the lab's fade into Sandgem (r44, "dialog"), and settle's samples
//  1.3 s apart can fall either side of it, so watch at settle's own darkness.
DpadPosition dpad_toward(Direction direction){
    return
        direction == Direction::NORTH ? DPAD_UP :
        direction == Direction::SOUTH ? DPAD_DOWN :
        direction == Direction::WEST ? DPAD_LEFT : DPAD_RIGHT;
}

bool run_into_door(VideoStream& stream, ProControllerContext& context, Direction direction, bool run){
    const DpadPosition dpad = dpad_toward(direction);
    BlackScreenWatcher dark(COLOR_RED, {0.1, 0.1, 0.8, 0.8}, 3 * FADE_DARKNESS, 60);
    ShortDialogWatcher dialog(COLOR_YELLOW);
    const int ret = run_until<ProControllerContext>(stream, context, [&](ProControllerContext& context){
        if (run){
            ssf_press_button(context, BUTTON_B, 0ms, 12000ms, 0ms);
        }
        pbf_press_dpad(context, dpad, 12000ms, 0ms);
    }, {{dark}, {dialog}});
    return ret == 0;
}

//  What the screen showed after a tap toward a door or a cave mouth.
enum class TapLook{
    NOTHING,
    DARK,
    BANNER,
};

//  Looks for `map`'s location banner (and, if `dark_too`, the fade) for up
//  to `wait`.
TapLook look_after_tap(VideoStream& stream, ProControllerContext& context, const std::string& map, Milliseconds wait, bool dark_too){
    const WallClock deadline = current_time() + wait;
    while (true){
        context.wait_for_all_requests();
        const VideoSnapshot screen = stream.video().snapshot();
        if (screen && location_banner_matches(read_location_banner(*screen.frame), map)){
            return TapLook::BANNER;
        }
        if (dark_too && screen && image_average(*screen.frame).sum() / 3 < FADE_DARKNESS){
            return TapLook::DARK;
        }
        if (current_time() >= deadline){
            return TapLook::NOTHING;
        }
        context.wait_for(100ms);
    }
}

//  A STEP "to_banner": D-pad taps one at a time toward a door or cave mouth,
//  each followed by a look, until the fade (then no more taps) and
//  pose_after's map's location banner. A held walk can carry on past a
//  fade that goes unseen: out of the Gym it walked on south (r219g), and
//  through the Oreburgh Gate's west mouth on west into a pocket by Route
//  203's pond (r75). Taps during the fade are eaten. No banner fails the
//  step right there.
Outcome tap_to_banner(VideoStream& stream, ProControllerContext& context, const StoryStep& step, const std::string& checkpoint){
    const std::string map = step.pose_after ? story_banner_name(Pokemon::grid_pose_of(*step.pose_after).map) : "";
    const Direction direction = story_arg<Direction>(step.args, "direction");
    for (size_t tap = 1; tap <= 30; tap++){
        pbf_press_dpad(context, dpad_toward(direction), 160ms, 0ms);
        TapLook look = look_after_tap(stream, context, map, 1500ms, true);
        const bool faded = look == TapLook::DARK;
        if (faded){
            look = look_after_tap(stream, context, map, 8000ms, false);
        }
        if (look == TapLook::BANNER){
            stream.log(std::format("[{}] {}: {}'s banner after {} taps {}.", checkpoint, step.name, map, tap, direction_name(direction)));
            context.wait_for(1000ms);
            return Pokemon::outcome_ok();
        }
        if (faded){
            break;
        }
    }
    return Pokemon::outcome_failed("no_banner", map);
}

} // namespace

Outcome StoryExecutor::step(RunState& state, const StoryStep& step){
    const Direction direction = story_arg<Direction>(step.args, "direction");
    route_turn(stream(), context(), direction);
    if (step.action == "to_banner"){
        return tap_to_banner(stream(), context(), step, m_checkpoint);
    }
    //  A step through a door: walk (or run) on until the screen goes dark,
    //  from next to the door or from anywhere in its line (r43: tapping 8 tiles
    //  down the lab took 7 s), then settle for any scene on arrival (Mom's,
    //  00.01; Dawn's tour, 00.04). Without the fade, settle must see it itself
    //  (r33 was outside, "no fade").
    if (step.pose_after && Pokemon::grid_pose_of(*step.pose_after).map != state.pose.map){
        const bool faded = run_into_door(stream(), context(), direction, m_can_run);
        stream().log(std::format("[{}] {}: {} through the door: {}.", m_checkpoint, step.name, direction_name(direction), faded ? "faded" : "no fade yet"));
        settle(stream(), context(), BUTTON_B, label_of(step), !faded);
        return Pokemon::outcome_ok();
    }
    const auto [x, y] = dpad_of(direction);
    const WalkResult walk = walk_tiles_dpad(stream(), context(), run_from_wild_battle, {x, y}, 1, step.name);
    stream().log(std::format("[{}] {}: a step {}, {}.", m_checkpoint, step.name, direction_name(direction), walk_stop_name(walk.stop)));
    std::vector<TilePos> at;
    for (const TilePos& pos : Pokemon::tiles_of(state.pose)){
        at.push_back(pos.next(direction));
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose(state.pose.map, at, direction));
}

Outcome StoryExecutor::talk(RunState& state, const StoryStep& step){
    //  Face whom we talk to: a route ends facing its last leg, not its goal
    //  (r40 reached Mom facing north). They block the tile, so the tap only turns.
    if (const std::optional<Direction> facing = Pokemon::facing_of(state.pose)){
        route_turn(stream(), context(), *facing);
    }
    pbf_press_button(context(), BUTTON_A, 80ms, 200ms);
    context().wait_for_all_requests();
    //  Roark's first line in the mine comes after the camera zooms in on
    //  us, which can take some seconds (r98; the old event waited 8 s).
    ShortDialogWatcher dialog(COLOR_YELLOW);
    if (wait_until(stream(), context(), 8000ms, {{dialog}}) != 0){
        fail_step(stream(), label_of(step) + ": nobody answered.");
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::answer(RunState&, const StoryStep& step){
    wait_for_dialogue(stream(), context(), label_of(step));
    const bool yes = story_arg<bool>(step.args, "yes");
    pbf_press_button(context(), yes ? BUTTON_A : BUTTON_B, 80ms, 1000ms);
    context().wait_for_all_requests();
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::mash(RunState& state, const StoryStep& step){
    //  The step before may have played the scene already (a door step settles
    //  its arrival scene), so its first line is not required.
    ShortDialogWatcher dialog(COLOR_CYAN);
    if (wait_until(stream(), context(), 15000ms, {{dialog}}) != 0){
        stream().log(label_of(step) + ": no line within 15 s; the step before played it.", COLOR_ORANGE);
    }
    settle(stream(), context(), step.action == "a" ? BUTTON_A : BUTTON_B, label_of(step),
        step.pose_after && Pokemon::grid_pose_of(*step.pose_after).map != state.pose.map);
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::battle(RunState&, const StoryStep& step){
    //  "starter": the Starly battle at the lake (00.02). It opens on its own,
    //  with no talk, and the starter has no Poke Balls to throw; the scene
    //  goes on whoever wins.
    if (step.action == "starter"){
        fight_starly(stream(), context());
        return Pokemon::outcome_ok();
    }
    //  A loss sends us to a Pokemon Center, so ending on its floor means a
    //  loss - unless the floor check already passed where the battle began:
    //  Jubilife's yellow plaza reads as Center floor (63 %, r79 won Rowan's
    //  tag battle there and was called lost).
    const bool may_lose = story_arg<bool>(step.args, "may_lose");
    const bool center_before = !may_lose && in_pokemon_center(stream());
    if (!talk_or_fight(stream(), context(), label_of(step), TalkExpect::BATTLE, step.action == "doubles" ? BattleFormat::DOUBLES : BattleFormat::SINGLES)){
        fail_step(stream(), label_of(step) + ": talked but did not battle.");
    }
    if (center_before){
        stream().log(label_of(step) + ": the floor read as a Center's before the battle too; no loss check here.", COLOR_ORANGE);
    }else if (!may_lose && in_pokemon_center(stream())){
        fail_step(stream(), label_of(step) + ": lost the battle.");
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::train(RunState&, const StoryStep& step){
    const int64_t battles = story_arg<int64_t>(step.args, "battles");
    for (int64_t battle = 0; battle < battles; battle++){
        const std::string label = std::format("Training battle {}/{}", battle + 1, battles);
        route_status(stream(), label);
        battle_1_pokemon(stream(), context());
        fail_if_blacked_out(stream(), context(), label);
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::heal(RunState&, const StoryStep& step){
    //  From the planned tile below the door, run in like any door. The lit-
    //  doorway lineup is for when the run in finds no door: people on the
    //  way can leave the route unsure of the column (r59, Jubilife). Below
    //  Sandgem's door the lineup read 0.7 tile off and stepped us off it (r48).
    route_turn(stream(), context(), Direction::NORTH);
    const bool in = run_into_door(stream(), context(), Direction::NORTH, m_can_run);
    stream().log(std::format("[{}] {}: north into the Center: {}.", m_checkpoint, step.name, in ? "faded" : "no door; lining up by sight"));
    if (in){
        pbf_wait(context(), 1500ms);
    }else{
        enter_lit_door(stream(), context(), label_of(step));
    }
    talk_to_nurse(stream(), context(), label_of(step));
    //  Her last line ("We hope to see you again!") can still be up, and a
    //  line stops the run out at once (r62): B until none shows.
    ShortDialogDetector line;
    for (size_t presses = 0; presses < 10; presses++){
        context().wait_for_all_requests();
        const VideoSnapshot screen = stream().video().snapshot();
        if (!screen || !line.detect(*screen.frame)){
            break;
        }
        pbf_press_button(context(), BUTTON_B, 80ms, 700ms);
    }
    //  Out the same way: south until the fade. The walk out's indoor check
    //  read "not a room" a step from the counter and stopped inside (r50, r59).
    route_turn(stream(), context(), Direction::SOUTH);
    const bool out = run_into_door(stream(), context(), Direction::SOUTH, m_can_run);
    stream().log(std::format("[{}] {}: south out of the Center: {}.", m_checkpoint, step.name, out ? "faded" : "no fade yet"));
    settle(stream(), context(), BUTTON_B, label_of(step), !out);
    //  r143a's heal said it had walked out while still inside.
    context().wait_for_all_requests();
    VideoSnapshot screen = stream().video().snapshot();
    if (screen && indoors(*screen.frame)){
        fail_step(stream(), label_of(step) + ": still inside the Pokemon Center.");
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::save(RunState&, const StoryStep& step){
    if (step.action.empty()){
        save_game(m_env.env, context());
        return Pokemon::outcome_ok();
    }
    //  A row's start save, its action the map the card must name. With a
    //  map, save_game() throws only when the card names another one, having
    //  backed out unsaved. That stops the program: a reset would load an
    //  older save and run this row from wherever that is.
    try{
        save_game(m_env.env, context(), step.action);
    }catch (OperationFailedException& e){
        throw FatalProgramException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("[{}] Not at the checkpoint's start, so not saving: {}", m_checkpoint, e.message()),
            stream()
        );
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::game(RunState&, const StoryStep& step){
    return Pokemon::outcome_failed("unknown_event", step.action);
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
                StoryStep save_at_start = SAVE_AT_START;
                save_at_start.action = row.saves_in;
                Pokemon::Event save = Pokemon::story_event(save_at_start, executor);
                save.skip_if.push_back(Pokemon::condition("saved at the start", [&saved, id = row.id](RunState&){
                    return saved.contains(id);
                }));
                //  A save that fails is a failed first attempt: the retry
                //  resets and does not save again.
                save.act = [&executor, &saved, id = row.id, save_at_start](RunState& state){
                    saved.insert(id);
                    return executor.run(save_at_start, state);
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



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
