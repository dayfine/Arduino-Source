/*  BDSP AutoStory - Console story
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/FatalProgramException.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "Actions/PokemonBDSP_AutoStoryMenuActions.h"
#include "Actions/PokemonBDSP_AutoStoryCenter.h"
#include "Actions/PokemonBDSP_AutoStoryEvents.h"
#include "Actions/PokemonBDSP_AutoStoryLitDoorway.h"
#include "Actions/PokemonBDSP_AutoStoryNewGame.h"
#include "Actions/PokemonBDSP_AutoStorySettings.h"
#include "Actions/PokemonBDSP_AutoStoryTalks.h"
#include "Actions/PokemonBDSP_AutoStoryWildBattle.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryIndoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryRouteWalk.h"
#include "PokemonBDSP_AutoStoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::StoryFailure;


namespace{

//  "to_catch_grass" as "to catch grass", for the logs.
std::string label_of(const BdspStep& step){
    std::string label = step.name;
    std::replace(label.begin(), label.end(), '_', ' ');
    return label;
}

//  The Center visit on the console: a turn before each door run, the
//  lit-doorway lineup, the nurse, and the reads of the inside.
class ConsoleCenterVisit : public BdspCenterVisit{
public:
    ConsoleCenterVisit(VideoStream& stream, ProControllerContext& context, StoryStepIO& io, bool run, std::string_view map, std::string label)
        : BdspCenterVisit(io, run, map)
        , m_stream(stream)
        , m_context(context)
        , m_label(std::move(label))
    {}

    virtual WarpSeen walk_in() override{
        route_turn(m_stream, m_context, Direction::NORTH);
        return BdspCenterVisit::walk_in();
    }
    //  The lineup is for when the run in finds no door: people on the way
    //  can leave the route unsure of the column (r59, Jubilife). Below
    //  Sandgem's door the lineup read 0.7 tile off and stepped us off it (r48).
    virtual void line_up_by_sight() override{
        enter_lit_door(m_stream, m_context, m_label);
    }
    //  The Center's own views, or any room: the walk out's room check
    //  caught r143a still inside.
    virtual Pokemon::CenterPlace place() override{
        m_context.wait_for_all_requests();
        const VideoSnapshot screen = m_stream.video().snapshot();
        if (!screen){
            return Pokemon::CenterPlace::UNKNOWN;
        }
        const bool inside = pokemon_center_inside(*screen.frame) || indoors(*screen.frame);
        return inside ? Pokemon::CenterPlace::INSIDE : Pokemon::CenterPlace::OUTSIDE;
    }
    virtual void heal() override{
        talk_to_nurse(m_stream, m_context, m_label);
        //  Her last line ("We hope to see you again!") can still be up, and a
        //  line stops the run out at once (r62): B until none shows.
        ShortDialogDetector line;
        for (size_t presses = 0; presses < 10; presses++){
            m_context.wait_for_all_requests();
            const VideoSnapshot screen = m_stream.video().snapshot();
            if (!screen || !line.detect(*screen.frame)){
                break;
            }
            pbf_press_button(m_context, BUTTON_B, 80ms, 700ms);
        }
    }
    //  Out the same way: south until the fade. The walk out's indoor check
    //  read "not a room" a step from the counter and stopped inside (r50, r59).
    virtual WarpSeen walk_out() override{
        route_turn(m_stream, m_context, Direction::SOUTH);
        return BdspCenterVisit::walk_out();
    }
    virtual void log(std::string_view line) override{
        m_stream.log(std::string(line));
    }

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    std::string m_label;
};

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


void ConsoleStepGuard::before_step(const BdspStep& step){
    m_stream.log(std::format("[{}] {} {}: {}", m_checkpoint, step.action, step.name, step.why), COLOR_BLUE);
}

bool ConsoleStepGuard::stops_on_failure() const{
    return m_stop_rows.contains(m_checkpoint);
}

void ConsoleStepGuard::on_failure(const BdspStep&, const std::string& message){
    //  Attempts are the engine's policy; this only counts them for the
    //  log line the lab's run watcher reads.
    const size_t attempt = m_failures[m_checkpoint]++;
    m_stream.log(std::format("checkpoint_reattempt_loop: attempt {} failed: {}: {}", attempt, m_checkpoint, message), COLOR_RED);
    save_failure_screenshot(m_stream, attempt);
}

void ConsoleStepGuard::on_interrupt(const BdspStep& step, const Pokemon::InterruptCheck& check){
    const bool cleared = !check.failure;
    m_stream.log(std::format(
        "[{}] {}: before it, {}: {}", m_checkpoint, step.name,
        Pokemon::interrupt_kind_name(check.verdict.kind),
        cleared ? std::string("cleared") : std::format("{}", *check.failure)
    ), cleared ? COLOR_ORANGE : COLOR_RED);
}

void ConsoleStepGuard::on_screen(const BdspStep& step, const Pokemon::ScreenSettle& screen){
    m_stream.log(std::format("[Screen] before {}: {}", step.name, describe(screen)));
}


ConsoleStory::ConsoleStory(StoryEnv& env)
    : m_env(env)
    , m_io(env.env.console, env.context)
    , m_walks(m_io)
    , m_guard(env.env.console, m_checkpoint)
    , m_backend([&env](std::string_view line){ env.env.console.log(std::string(line), COLOR_BLUE); }, env.context)
    , m_look(env.env.console, env.context)
    , m_interrupts(story_interrupts([this](bool fresh){ return m_look.look(fresh); }, console_interrupt_actions()))
{}

std::optional<StoryFailure> ConsoleStory::run_wrapped_step(
    const BdspStep& step,
    BdspRunState& state,
    const std::function<std::optional<StoryFailure>()>& run_action
){
    const auto record_failure = [&](StoryFailure failure) -> StoryFailure{
        if (m_guard.stops_on_failure()){
            failure.repeats = true;
        }else{
            m_guard.on_failure(step, std::format("{}", failure));
        }
        return failure;
    };
    m_guard.before_step(step);
    const Pokemon::InterruptCheck check = Pokemon::clear_story_interrupt(m_interrupts, step.name, step.action);
    m_guard.on_screen(step, check.screen);
    if (check.pose_lost){
        state.pose = {};
    }
    if (check.verdict.kind != Pokemon::InterruptKind::NONE || check.failure){
        m_guard.on_interrupt(step, check);
    }
    if (check.failure){
        return record_failure(*check.failure);
    }
    try{
        std::optional<StoryFailure> failure = run_action();
        if (failure){
            return record_failure(*failure);
        }
        return std::nullopt;
    }catch (FatalProgramException&){
        throw;
    }catch (OperationFailedException& error){
        if (m_guard.stops_on_failure()){
            throw;
        }
        m_guard.on_failure(step, error.message());
        return StoryFailure{.code = "operation_failed", .detail = error.message()};
    }
}

BdspStoryHandlers ConsoleStory::console_handlers(){
    const auto method = [this](std::optional<StoryFailure> (ConsoleStory::*action)(BdspRunState&, const BdspStep&)){
        return [this, action](const BdspStep& step, BdspRunState& state){
            return (this->*action)(state, step);
        };
    };
    return BdspStoryHandlers{
        .new_game = method(&ConsoleStory::new_game),
        .options = method(&ConsoleStory::options),
        .answer = method(&ConsoleStory::answer),
        .battle = method(&ConsoleStory::battle),
        .train = method(&ConsoleStory::train),
        .heal = method(&ConsoleStory::heal),
        .save = method(&ConsoleStory::save),
        .event = [this](const std::string& name) -> BdspStepAction{
            return [this, event = story_events().at(name)](const BdspStep& step, BdspRunState& state){
                return event(m_env, state, step);
            };
        },
        .on_attempt_start = [this](const StoryCheckpoint& row){
            m_checkpoint = row.id;
        },
        .wrap_step = [this](
            const StoryCheckpoint&,
            const BdspStep& step,
            BdspRunState& state,
            const std::function<std::optional<StoryFailure>()>& run_action
        ){
            return run_wrapped_step(step, state, run_action);
        },
    };
}

//  A wild battle is run from, a trainer who saw us fought, a dialog or a
//  warp's scene pressed through to the quiet overworld.
StoryInterruptActions ConsoleStory::console_interrupt_actions(){
    const auto settle = [this](std::string label){
        return [this, label](const Pokemon::InterruptVerdict&) -> std::optional<StoryFailure>{
            m_io.settle(BUTTON_B, label, SettleKind::TALK, "");
            return std::nullopt;
        };
    };
    return StoryInterruptActions{
        .wild = [this](const Pokemon::InterruptVerdict&) -> std::optional<StoryFailure>{
            run_from_wild_battle(stream(), context());
            return std::nullopt;
        },
        .trainer = [this](const Pokemon::InterruptVerdict&) -> std::optional<StoryFailure>{
            m_io.fight("");
            return std::nullopt;
        },
        .dialog = settle("a dialog between steps"),
        .warp = settle("a warp between steps"),
    };
}

std::vector<Pokemon::StoryCheckpoint> ConsoleStory::make_checkpoints(std::span<const StoryCheckpoint> rows){
    return make_bdsp_engine_checkpoints(rows, m_walks, m_run_state, console_handlers());
}

std::optional<StoryFailure> ConsoleStory::save_checkpoint(const std::string& saves_in){
    if (saves_in.empty()){
        save_game(m_env.env, context());
        return std::nullopt;
    }
    //  Check the save card before saving. With a map, save_game() throws only
    //  when the card names another one, having backed out unsaved. That stops
    //  the program: a reset would load an older save and run the next row from
    //  wherever that is.
    try{
        save_game(m_env.env, context(), saves_in);
    }catch (OperationFailedException& e){
        throw FatalProgramException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("[{}] Not at the checkpoint's expected map, so not saving: {}", m_checkpoint, e.message()),
            stream()
        );
    }
    return std::nullopt;
}

std::optional<StoryFailure> ConsoleStory::new_game(BdspRunState&, const BdspStep&){
    play_new_game_intro(m_env.env, context(), m_env.intro_picks, m_env.player_name);
    return std::nullopt;
}

std::optional<StoryFailure> ConsoleStory::options(BdspRunState&, const BdspStep&){
    verify_tutorial_settings(stream(), context());
    context().wait_for_all_requests();
    return std::nullopt;
}

std::optional<StoryFailure> ConsoleStory::answer(BdspRunState&, const BdspStep& step){
    wait_for_dialogue(stream(), context(), label_of(step));
    const bool yes = std::get<AnswerStepArgs>(step.args).yes;
    pbf_press_button(context(), yes ? BUTTON_A : BUTTON_B, 80ms, 1000ms);
    context().wait_for_all_requests();
    return std::nullopt;
}

std::optional<StoryFailure> ConsoleStory::battle(BdspRunState&, const BdspStep& step){
    const BattleStepArgs& args = std::get<BattleStepArgs>(step.args);
    //  "starter": the Starly battle at the lake (00.02). It opens on its own,
    //  with no talk, and the starter has no Poke Balls to throw; the scene
    //  goes on whoever wins.
    if (args.variant == "starter"){
        fight_starly(stream(), context());
        return std::nullopt;
    }
    //  A loss sends us to a Pokemon Center, so ending on its floor means a
    //  loss - unless the floor check already passed where the battle began:
    //  Jubilife's yellow plaza reads as Center floor (63 %, r79 won Rowan's
    //  tag battle there and was called lost).
    const bool center_before = !args.may_lose && in_pokemon_center(stream());
    const BattleFormat format = args.variant == "doubles" ? BattleFormat::DOUBLES : BattleFormat::SINGLES;
    //  The plan's foe was checked when it was read.
    const FoeTeam foe = args.foe ? known_foe_team(*args.foe).value_or(FoeTeam{}) : FoeTeam{};
    if (!talk_or_fight(stream(), context(), label_of(step), TalkExpect::BATTLE, format, foe)){
        fail_step(stream(), label_of(step) + ": talked but did not battle.");
    }
    if (center_before){
        stream().log(label_of(step) + ": the floor read as a Center's before the battle too; no loss check here.", COLOR_ORANGE);
    }else if (!args.may_lose && in_pokemon_center(stream())){
        fail_step(stream(), label_of(step) + ": lost the battle.");
    }
    return std::nullopt;
}

std::optional<StoryFailure> ConsoleStory::train(BdspRunState&, const BdspStep& step){
    const int64_t battles = std::get<TrainStepArgs>(step.args).battles;
    for (int64_t battle = 0; battle < battles; battle++){
        const std::string label = std::format("Training battle {}/{}", battle + 1, battles);
        route_status(stream(), label);
        battle_1_pokemon(stream(), context());
        fail_if_blacked_out(stream(), context(), label);
    }
    return std::nullopt;
}

std::optional<StoryFailure> ConsoleStory::heal(BdspRunState& state, const BdspStep& step){
    //  From the planned tile below the door, run in like any door, and out
    //  again to the tile below it. With no fade, the run can have gone on in
    //  to the counter (lab-B 2026-11-08, Oreburgh).
    ConsoleCenterVisit visit(stream(), context(), m_io, m_walks.can_run(), state.pose.map, label_of(step));
    return Pokemon::run_center_visit(std::format("[{}] {}", m_checkpoint, step.name), visit);
}

std::optional<StoryFailure> ConsoleStory::save(BdspRunState&, const BdspStep& step){
    const SaveStepArgs& args = std::get<SaveStepArgs>(step.args);
    return save_checkpoint(args.saves_in);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
