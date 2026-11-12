/*  BDSP AutoStory - Console story
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>
#include <variant>
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
#include "Actions/PokemonBDSP_AutoStory_LitDoorway.h"
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
#include "PokemonBDSP_AutoStory_ConsoleStory.h"

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

} // namespace


void ConsoleStepGuard::before_step(const StoryStep& step){
    m_stream.log(std::format("[{}] {} {}: {}", m_checkpoint, Pokemon::story_step_kind_name(step.kind), step.name, step.why), COLOR_BLUE);
}

bool ConsoleStepGuard::stops_on_failure(const StoryStep&) const{
    return m_stop_rows.contains(m_checkpoint);
}

void ConsoleStepGuard::on_failure(const StoryStep&, const OperationFailedException& error){
    //  Attempts are the segment's policy; this only counts them for the
    //  log line the lab's run watcher reads.
    const size_t attempt = m_failures[m_checkpoint]++;
    m_stream.log(std::format("checkpoint_reattempt_loop: attempt {} failed: {}: {}", attempt, m_checkpoint, error.message()), COLOR_RED);
    save_failure_screenshot(m_stream, attempt);
}

void ConsoleStepGuard::on_interrupt(const StoryStep& step, const Pokemon::InterruptCheck& check){
    const bool cleared = check.outcome.succeeded();
    m_stream.log(std::format(
        "[{}] {}: before it, {}: {}", m_checkpoint, step.name,
        Pokemon::interrupt_kind_name(check.verdict.kind), cleared ? "cleared" : Pokemon::to_string(check.outcome)
    ), cleared ? COLOR_ORANGE : COLOR_RED);
}

void ConsoleStepGuard::on_screen(const StoryStep& step, const Pokemon::ScreenSettle& screen){
    m_stream.log(std::format("[Screen] before {}: {}", step.name, Pokemon::describe(screen)));
}


//  The table and the guard are members, built after the base: it only
//  keeps references to them until a step runs.
ConsoleStory::ConsoleStory(StoryEnv& env)
    : Pokemon::TableExecutor(m_no_table, m_guard)
    , m_env(env)
    , m_io(env.env.console, env.context)
    , m_walks(m_io)
    , m_guard(env.env.console, m_checkpoint)
    , m_backend(Pokemon::StoryBackendKind::CONSOLE, [&env](std::string_view line){ env.env.console.log(std::string(line)); }, env.context)
    , m_steps(m_walks, console_actions(), m_backend)
    , m_look(env.env.console, env.context)
    , m_interrupts(story_interrupts([this](bool fresh){ return m_look.look(fresh); }, console_interrupt_actions()))
{
    set_interrupts(&m_interrupts);
}

StoryBoundActions ConsoleStory::console_actions(){
    const auto method = [this](Outcome (ConsoleStory::*action)(RunState&, const StoryStep&)){
        return [this, action](const StoryStep& step, RunState& state){
            return (this->*action)(state, step);
        };
    };
    return StoryBoundActions{
        .new_game = method(&ConsoleStory::new_game),
        .options = method(&ConsoleStory::options),
        .answer = method(&ConsoleStory::answer),
        .battle = method(&ConsoleStory::battle),
        .train = method(&ConsoleStory::train),
        .heal = method(&ConsoleStory::heal),
        .save = method(&ConsoleStory::save),
        .event = [this](const std::string& name) -> StoryBoundAction{
            return [this, event = story_events().at(name)](const StoryStep& step, RunState& state){
                return event(m_env, state, step);
            };
        },
    };
}

//  A wild battle is run from, a trainer who saw us fought, a dialog or a
//  warp's scene pressed through to the quiet overworld.
StoryInterruptActions ConsoleStory::console_interrupt_actions(){
    const auto settle = [this](std::string label){
        return [this, label](const Pokemon::InterruptVerdict&, RunState&){
            m_io.settle(BUTTON_B, label, SettleKind::TALK, "");
            return Pokemon::outcome_ok();
        };
    };
    return StoryInterruptActions{
        .wild = [this](const Pokemon::InterruptVerdict&, RunState&){
            run_from_wild_battle(stream(), context());
            return Pokemon::outcome_ok();
        },
        .trainer = [this](const Pokemon::InterruptVerdict&, RunState&){
            m_io.fight("");
            return Pokemon::outcome_ok();
        },
        .dialog = settle("a dialog between steps"),
        .warp = settle("a warp between steps"),
    };
}

void ConsoleStory::begin_attempt(const Pokemon::StoryCheckpoint& data){
    m_checkpoint = data.id;
    m_walks.begin_attempt(data.id, data.steps);
}

Outcome ConsoleStory::run_action(const StoryStep& step, RunState& state){
    return m_steps.act(step, state);
}


Outcome ConsoleStory::new_game(RunState&, const StoryStep&){
    play_new_game_intro(m_env.env, context(), m_env.player_name);
    return Pokemon::outcome_ok();
}

Outcome ConsoleStory::options(RunState&, const StoryStep&){
    verify_tutorial_settings(stream(), context());
    context().wait_for_all_requests();
    return Pokemon::outcome_ok();
}

Outcome ConsoleStory::answer(RunState&, const StoryStep& step){
    wait_for_dialogue(stream(), context(), label_of(step));
    const bool yes = story_arg<bool>(step.args, "yes");
    pbf_press_button(context(), yes ? BUTTON_A : BUTTON_B, 80ms, 1000ms);
    context().wait_for_all_requests();
    return Pokemon::outcome_ok();
}

Outcome ConsoleStory::battle(RunState&, const StoryStep& step){
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

Outcome ConsoleStory::train(RunState&, const StoryStep& step){
    const int64_t battles = story_arg<int64_t>(step.args, "battles");
    for (int64_t battle = 0; battle < battles; battle++){
        const std::string label = std::format("Training battle {}/{}", battle + 1, battles);
        route_status(stream(), label);
        battle_1_pokemon(stream(), context());
        fail_if_blacked_out(stream(), context(), label);
    }
    return Pokemon::outcome_ok();
}

Outcome ConsoleStory::heal(RunState&, const StoryStep& step){
    //  From the planned tile below the door, run in like any door. The lit-
    //  doorway lineup is for when the run in finds no door: people on the
    //  way can leave the route unsure of the column (r59, Jubilife). Below
    //  Sandgem's door the lineup read 0.7 tile off and stepped us off it (r48).
    //  With no fade, the run can have gone on in to the counter (lab-B
    //  2026-11-08, Oreburgh).
    route_turn(stream(), context(), Direction::NORTH);
    const CenterEntry in = run_into_center(m_io, m_walks.can_run(), [this]{
        context().wait_for_all_requests();
        const VideoSnapshot screen = stream().video().snapshot();
        return screen && pokemon_center_inside(*screen.frame);
    }, [&]{
        enter_lit_door(stream(), context(), label_of(step));
    }, label_of(step));
    stream().log(std::format("[{}] {}: north into the Center: {}.", m_checkpoint, step.name,
        in == CenterEntry::FADED ? "faded" : in == CenterEntry::INSIDE ? "no fade, but inside" : "no fade; lined up by sight"));
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
    const bool out = m_io.run_into_door(Direction::SOUTH, m_walks.can_run()).any();
    stream().log(std::format("[{}] {}: south out of the Center: {}.", m_checkpoint, step.name, out ? "faded" : "no fade yet"));
    if (out){
        m_io.settle(BUTTON_B, label_of(step), SettleKind::DOOR, "");
    }else{
        m_io.settle(BUTTON_B, label_of(step), SettleKind::AFTER_WARP, "");
    }

    //  r143a's heal said it had walked out while still inside.
    context().wait_for_all_requests();
    VideoSnapshot screen = stream().video().snapshot();
    if (screen && indoors(*screen.frame)){
        fail_step(stream(), label_of(step) + ": still inside the Pokemon Center.");
    }
    return Pokemon::outcome_ok();
}

Outcome ConsoleStory::save(RunState&, const StoryStep& step){
    const auto saves_in = step.args.find(SAVES_IN_ARG);
    const std::string* map = saves_in == step.args.end() ? nullptr : std::get_if<std::string>(&saves_in->second);
    if (map == nullptr || map->empty()){
        save_game(m_env.env, context());
        return Pokemon::outcome_ok();
    }
    //  A row's start save, its SAVES_IN_ARG the map the card must name. With
    //  a map, save_game() throws only when the card names another one, having
    //  backed out unsaved. That stops the program: a reset would load an
    //  older save and run this row from wherever that is.
    try{
        save_game(m_env.env, context(), *map);
    }catch (OperationFailedException& e){
        throw FatalProgramException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("[{}] Not at the checkpoint's start, so not saving: {}", m_checkpoint, e.message()),
            stream()
        );
    }
    return Pokemon::outcome_ok();
}





} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
