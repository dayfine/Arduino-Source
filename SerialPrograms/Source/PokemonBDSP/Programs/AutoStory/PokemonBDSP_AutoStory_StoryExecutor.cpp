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
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Actions.h"
#include "PokemonBDSP_AutoStory_Center.h"
#include "PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP_AutoStory_Settings.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
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
    if (variant == "line"){
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

}


StoryExecutor::StoryExecutor(StoryEnv& env)
    : m_env(env)
    , m_io(env.env.console, env.context)
    , m_walks(m_io)
{}

void StoryExecutor::begin_attempt(const Pokemon::StoryCheckpoint& data){
    m_checkpoint = data.id;
    m_walks.begin_attempt(data.id);
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
    return m_walks.route(state, step);
}

Outcome StoryExecutor::legs(RunState& state, const StoryStep& step){
    return m_walks.legs(state, step);
}

Outcome StoryExecutor::step(RunState& state, const StoryStep& step){
    return m_walks.step(state, step);
}

Outcome StoryExecutor::talk(RunState& state, const StoryStep& step){
    return m_walks.talk(state, step);
}

Outcome StoryExecutor::answer(RunState&, const StoryStep& step){
    wait_for_dialogue(stream(), context(), label_of(step));
    pbf_press_button(context(), step.yes ? BUTTON_A : BUTTON_B, 80ms, 1000ms);
    context().wait_for_all_requests();
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::mash(RunState& state, const StoryStep& step){
    return m_walks.mash(state, step);
}

Outcome StoryExecutor::battle(RunState&, const StoryStep& step){
    //  "starter": the Starly battle at the lake (00.02). It opens on its own,
    //  with no talk, and the starter has no Poke Balls to throw; the scene
    //  goes on whoever wins.
    if (step.variant == "starter"){
        fight_starly(stream(), context());
        return Pokemon::outcome_ok();
    }
    //  A loss sends us to a Pokemon Center, so ending on its floor means a
    //  loss - unless the floor check already passed where the battle began:
    //  Jubilife's yellow plaza reads as Center floor (63 %, r79 won Rowan's
    //  tag battle there and was called lost).
    const bool center_before = !step.may_lose && in_pokemon_center(stream());
    if (!talk_or_fight(stream(), context(), label_of(step), TalkExpect::BATTLE, step.variant == "doubles" ? BattleFormat::DOUBLES : BattleFormat::SINGLES)){
        fail_step(stream(), label_of(step) + ": talked but did not battle.");
    }
    if (center_before){
        stream().log(label_of(step) + ": the floor read as a Center's before the battle too; no loss check here.", COLOR_ORANGE);
    }else if (!step.may_lose && in_pokemon_center(stream())){
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
    //  From the planned tile below the door, run in like any door. The lit-
    //  doorway lineup is for when the run in finds no door: people on the
    //  way can leave the route unsure of the column (r59, Jubilife). Below
    //  Sandgem's door the lineup read 0.7 tile off and stepped us off it (r48).
    route_turn(stream(), context(), Direction::NORTH);
    const bool in = m_io.run_into_door(Direction::NORTH, m_walks.can_run());
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
    const bool out = m_io.run_into_door(Direction::SOUTH, m_walks.can_run());
    stream().log(std::format("[{}] {}: south out of the Center: {}.", m_checkpoint, step.name, out ? "faded" : "no fade yet"));
    m_io.settle(BUTTON_B, label_of(step), !out);
    //  r143a's heal said it had walked out while still inside.
    context().wait_for_all_requests();
    VideoSnapshot screen = stream().video().snapshot();
    if (screen && indoors(*screen.frame)){
        fail_step(stream(), label_of(step) + ": still inside the Pokemon Center.");
    }
    return Pokemon::outcome_ok();
}

Outcome StoryExecutor::save(RunState&, const StoryStep& step){
    if (step.variant.empty()){
        save_game(m_env.env, context());
        return Pokemon::outcome_ok();
    }
    //  A row's start save, its variant the map the card must name. With a
    //  map, save_game() throws only when the card names another one, having
    //  backed out unsaved. That stops the program: a reset would load an
    //  older save and run this row from wherever that is.
    try{
        save_game(m_env.env, context(), step.variant);
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
                StoryStep save_at_start = SAVE_AT_START;
                save_at_start.variant = row.saves_in;
                Pokemon::Event save = Pokemon::story_event(save_at_start, executor);
                save.skip_if.push_back(Pokemon::condition("saved at the start", [&saved, id = row.id](RunState&){
                    return saved.contains(id);
                }));
                //  A save that fails is a failed first attempt: the retry
                //  resets and does not save again.
                save.act = [&executor, &saved, id = row.id, save_at_start](RunState& state){
                    saved.insert(id);
                    return executor.run(state, save_at_start);
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
