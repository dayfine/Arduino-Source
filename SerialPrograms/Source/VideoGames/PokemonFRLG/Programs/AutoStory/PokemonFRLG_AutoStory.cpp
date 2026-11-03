/*  AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_NamingScreenKeyboard.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_TrainerCard.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanLog.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTracker.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SidSteps.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngConsoleSteps.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRngLoop.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:AutoStory",
        Pokemon::STRING_POKEMON + " FRLG", "AutoStory",
        "",
        "Play FireRed (English) from a new game to the Boulder Badge.",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

std::unique_ptr<StatsTracker> AutoStory_Descriptor::make_stats() const{
    return std::make_unique<Pokemon::AutoStoryStats>();
}


namespace{

//  The start and end dropdowns' entries: the table's checkpoints, 00.01 to
//  00.11. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = Pokemon::story_checkpoint_database(story_checkpoints());
    return database;
}

}

const char* const RUN_STARTER_RNG_TEXT =
    "<b>Run Starter RNG in AutoStory:</b><br>Off by default. Instead of stopping for them, run SID Helper's steps on the "
    "new game's keyboard when the RNG plan has no SID, and Starter RNG's attempts from a save south of the table at the "
    "end of 00.04, as the RNG plan lists them. A shiny Squirtle goes on to the rival's battle, saves after it and the run "
    "goes on; otherwise the run stops with the save south of the table.<br>"
    "SID Helper's timing needs the text speed and button settings already saved, as with Stop for SID Helper.";

AutoStory::AutoStory()
    : RUN(checkpoint_database())
    , NEW_GAME_FROM_TITLE(
        "<b>New game from the title screen:</b><br>The game is on its title screen with no save data. "
        "Needed to start at 00.01. Otherwise the program loads the save, which must be the one the start checkpoint begins from.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , PLAYER_NAME(
        false,
        "<b>Player name:</b><br>Typed on the new game's keyboard (00.01). Empty keeps the game's default name. "
        "The rival always keeps the default name.",
        LockMode::LOCK_WHILE_RUNNING,
        "Kosmos", ""
    )
    , STOP_FOR_SID_HELPER(
        "<b>Stop for SID Helper:</b><br>Off by default. At 00.01, stop on the player's keyboard with the name typed and OK not pressed, "
        "where the SID Helper program starts. After SID Helper, start AutoStory at 00.01b, with the game as SID Helper left it "
        "(the Trainer Card, or HOME): it closes the card, sets the options and saves in the bedroom.<br>"
        "SID Helper needs the text speed and button settings already saved. On a cartridge with that save, choose NEW GAME "
        "by hand and start AutoStory on the first message.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , STOP_FOR_STARTER_RNG(
        "<b>Stop for Starter RNG:</b><br>Off by default. At the end of 00.04, save south of the table facing Squirtle's ball "
        "and stop, where the Starter RNG program starts. After Starter RNG, save right after the rival's battle without moving "
        "and start AutoStory at 00.05, listing the targets to try from the RNG plan.<br>"
        "With this off, 00.04 and 00.04b run as one checkpoint, as before.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , RUN_STARTER_RNG(RUN_STARTER_RNG_TEXT, LockMode::LOCK_WHILE_RUNNING, false)
    , NOTIFICATION_SHINY("Shiny found", true, true, ImageAttachmentMode::JPG, {"Notifs", "Showcase"})
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({&NOTIFICATION_SHINY, &NOTIFICATION_STATUS_UPDATE, &NOTIFICATION_PROGRAM_FINISH, &NOTIFICATION_ERROR_FATAL})
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(NEW_GAME_FROM_TITLE);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(STOP_FOR_SID_HELPER);
    PA_ADD_OPTION(STOP_FOR_STARTER_RNG);
    PA_ADD_OPTION(RUN_STARTER_RNG);
    PA_ADD_OPTION(RNG_PLAN);
    PA_ADD_OPTION(NOTIFICATIONS);
}


//  The save loaded must be the one checkpoint `first` starts from: one left
//  by an earlier checkpoint (a run that failed before saving) would start
//  it in the wrong place (console B, 2026-11-01: 00.09 on the forest's
//  save). Checked on the SAVE card; a card that can't be read is let pass.
static void check_start_save(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, const StoryCheckpoint& checkpoint
){
    if (!checkpoint.start){
        return;
    }
    const std::string error = check_save_place(story, checkpoint.start->map);
    if (!error.empty()){
        throw UserSetupError(env.console, error + " Start at the checkpoint " + checkpoint.id + " the save is for.");
    }
}

namespace{

//  Before the new game starts: a name the keyboard can't type is the user's to fix.
void check_player_name(ConsoleHandle& console, const std::string& name){
    if (name.empty()){
        return;
    }
    const std::string error = naming_text_error(name, NamingScreenKind::PLAYER);
    if (!error.empty()){
        throw UserSetupError(console, "Player name: " + error);
    }
}

//  A failed attempt reloads the last save, with the supplies it had; the
//  engine's lines go to the console's log.
Pokemon::SegmentHooks story_hooks(SingleSwitchProgramEnvironment& env, StoryEnv& story){
    Pokemon::SegmentHooks hooks;
    hooks.reload = [&story](Pokemon::RunState&){
        if (!story.has_save){
            return Pokemon::outcome_failed("no_save", "nothing saved yet");
        }
        //  The save reloaded must be where the checkpoint starts: a save
        //  that didn't happen leaves an older one (console B, 2026-11-02:
        //  00.02 walked the bedroom's route in Pewter City).
        try{
            soft_reset(story.console, story.context);
            const std::string error = check_save_place(story, story.start_map);
            if (!error.empty()){
                return Pokemon::outcome_failed("wrong_save", error);
            }
        }catch (OperationFailedException& e){
            return Pokemon::outcome_failed("reload_failed", e.message());
        }
        story.supplies = story.saved_supplies;
        return Pokemon::outcome_ok();
    };
    hooks.log = [&env](const std::string& line){
        env.console.log(line);
    };
    //  The checkpoint and the step under way, for whoever watches the stream.
    hooks.status = [&env](const std::string& line){
        env.console.overlay().add_log(line);
    };
    return hooks;
}


}

//  The options' problems, before anything is pressed.
static void check_setup(
    ConsoleHandle& console, const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last,
    bool new_game_from_title, const std::string& player_name
){
    if (first > last || last >= checkpoints.size()){
        throw UserSetupError(console, "The start checkpoint must not come after the end checkpoint.");
    }
    if (first == 0 && !new_game_from_title){
        throw UserSetupError(console, "Checkpoint 00.01 starts a new game: start on the title screen with no save.");
    }
    if (first == 0){
        check_player_name(console, player_name);
    }
}

static void throw_if_failed(const Pokemon::SegmentReport& report){
    if (report.completed){
        return;
    }
    throw OperationFailedException(
        ErrorReportMode::NO_ERROR_REPORT,
        std::format(
            "AutoStory stopped at {} {}: {}",
            report.failed_checkpoint, report.failed_event, Pokemon::to_string(report.failure)
        )
    );
}

//  Gets the game to where checkpoint `first` starts: a new game from the
//  title screen, the game as another program left it, or the checkpoint's
//  save loaded.
static void prepare_start(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, StoryEnv& story
){
    const StoryCheckpoint& checkpoint = checkpoints[first];
    if (!checkpoint.start){
        return;
    }
    if (checkpoint.live_start){
        env.console.log(checkpoint.id + " starts from the game as it is: no save to load.");
        return;
    }
    env.console.log("Loading the save for " + checkpoint.id + ".");
    soft_reset(env.console, context);
    story.has_save = true;
    check_start_save(env, story, checkpoints[first]);
}

//  The run stopped for an RNG program, as its option asked: the user's
//  turn, with what to do next, and the lines that go with it (the RNG
//  plan's attempts).
static void stop_for_handoff(
    SingleSwitchProgramEnvironment& env, EventNotificationOption& notification,
    const std::string& message, const std::vector<std::string>& lines = {}
){
    env.console.log(message, COLOR_BLUE);
    env.console.overlay().add_log(message);
    std::string full = message;
    for (const std::string& line : lines){
        env.console.log(line, COLOR_BLUE);
        env.console.overlay().add_log(line);
        full += "\n" + line;
    }
    send_program_finished_notification(env, notification, full);
}

static void stop_for_sid_helper(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    EventNotificationOption& notification, const std::string& player_name
){
    play_new_game_to_player_name(env.console, context, player_name);
    stop_for_handoff(env, notification,
        "Stopped for SID Helper: the player's name is typed and OK is not pressed. Run SID Helper now, "
        "enter its Target Advances and # Candidate SIDs in the RNG plan, then start AutoStory at 00.01b."
    );
}

//  The TID for the Starter RNG plan: the RNG plan's, or read off the
//  Trainer Card (START, the player's entry), which is then closed again.
//  Before the first Pokemon, as under the ball, the player's entry is second.
static uint16_t starter_rng_tid(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngPlanOptions& options, Language language
){
    std::optional<uint16_t> tid = options.tid_override(env.console);
    if (tid){
        env.console.log(rng_log_tid(*tid, false));
        return *tid;
    }
    navigate_to_trainer_card(env.console, context);
    tid = try_read_trainer_card_tid(env.console, context, language);
    if (!tid){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "Couldn't read the TID on the Trainer Card, so there is no Starter RNG plan. Nothing was saved: "
            "type the TID into the RNG plan's TID option and start AutoStory at 00.04 again.",
            env.console
        );
    }
    close_trainer_card(env.console, context);
    env.console.log(rng_log_tid(*tid, true));
    return *tid;
}

//  The Starter RNG attempts. Run under the ball before 00.04 saves there, so
//  a failure ends the run with 00.03's save kept.
static StarterRngPlanned plan_starter_rng_attempts(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngPlanOptions& options, Language language
){
    const uint16_t tid = starter_rng_tid(env, context, options, language);
    try{
        return StarterRngPlanned{.tid = tid, .attempts = options.plan(env.console, language, tid)};
    }catch (UserSetupError& e){
        throw UserSetupError(env.console, e.message() + " Nothing was saved: fix the RNG plan and start AutoStory at 00.04 again.");
    }
}

//  Has the last checkpoint of `segment` call `before` just before it saves.
static void before_last_save(Pokemon::Segment& segment, std::function<void()> before){
    Pokemon::CheckpointSpec& spec = segment.checkpoints.back();
    spec.build = [build = std::move(spec.build), before]() mutable{
        Pokemon::Checkpoint checkpoint = build();
        checkpoint.save = [save = std::move(checkpoint.save), before](Pokemon::RunState& state) mutable{
            before();
            return save(state);
        };
        return checkpoint;
    };
}

//  The attempts are the user's to run, with Run Starter RNG in AutoStory off.
//  TODO: LeafGreen and Bulbasaur/Charmander, with AutoStory's starter choice.
static void stop_for_starter_rng(
    SingleSwitchProgramEnvironment& env, EventNotificationOption& notification,
    const std::string& sound, const std::vector<std::string>& attempts
){
    stop_for_handoff(env, notification,
        "Stopped for Starter RNG: saved south of the table, facing Squirtle's ball. "
        "Run Starter RNG (Squirtle, FireRed, Sound " + sound + ") with attempt 1's Target Seed and Advances. "
        "If it hits the target and it isn't shiny, load this save and run the next attempt. If it is shiny, "
        "its SID is the one listed (or one of them): enter it as the RNG plan's SID. "
        "Then save right after the rival's battle, without moving, and start AutoStory at 00.05.",
        attempts
    );
}

static size_t checkpoint_index(const std::vector<StoryCheckpoint>& checkpoints, const std::string& id){
    for (size_t c = 0; c < checkpoints.size(); c++){
        if (checkpoints[c].id == id){
            return c;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint " + id + ".");
}

//  Runs `rows` as one segment, calling `before` (if set) just before the
//  last row saves. Throws if a checkpoint fails for good.
static void run_story_rows(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, StoryExecutor& executor,
    const std::vector<StoryCheckpoint>& rows, const Pokemon::SegmentPolicy& policy,
    std::function<void()> before
){
    Pokemon::Segment segment = make_story_segment(rows, executor);
    if (before){
        before_last_save(segment, std::move(before));
    }
    Pokemon::SegmentHooks hooks = story_hooks(env, story);

    Pokemon::RunState state;
    const Pokemon::SegmentReport report = run_segment(
        state, segment, Pokemon::SegmentRange{rows.front().id, rows.back().id}, policy, hooks
    );
    Pokemon::record_story_stats(env.current_stats<Pokemon::AutoStoryStats>(), report);
    env.update_stats();
    throw_if_failed(report);
}

//  SID Helper's steps in the program, from the player's keyboard: the name
//  typed, then the intro at SID Helper's timing into the bedroom, and the
//  Trainer Card opened to read the TID. Ends on the card, where 00.01b
//  starts.
static void set_sid_in_program(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& player_name, const StarterRngPlanOptions& options, Language language
){
    const uint32_t target_advances = options.SID_HELPER_ADVANCES;
    if (target_advances % 2 == 0){
        throw UserSetupError(env.console, "RNG plan: SID Helper's Target Advances must be odd.");
    }
    play_new_game_to_player_name(env.console, context, player_name);
    const SidTiming timing = sid_timing(language, target_advances);
    env.log("SID Helper's steps: delay " + std::to_string(timing.delay_ms) + "ms");
    set_sid_from_name_screen(context, timing);
    finish_intro_animations(env, context);

    navigate_to_trainer_card(env.console, context);
    const std::optional<uint16_t> tid = try_read_trainer_card_tid(env.console, context, language);
    if (!tid){
        env.console.log("Couldn't read the TID on the Trainer Card; 00.04 reads it again.", COLOR_ORANGE);
        return;
    }
    env.console.log(rng_log_tid(*tid, true));
    env.console.log(rng_log_sid_helper(*tid, target_advances, options.helper_candidates(*tid)));
}

namespace{

//  Run Starter RNG in AutoStory: the time caps, Starter RNG's default Max
//  Resets per try, and an attempt's misses before the run stops.
const StarterRngLoopLimits STARTER_RNG_LIMITS{
    .attempt_time = std::chrono::hours(3),
    .total_time = std::chrono::hours(12),
    .max_resets = 50,
    .max_retries = 2,
};

}

//  Starter RNG's attempts in the program, from the save south of the table.
//  Returns true for a shiny Squirtle, on the target or not, the game on its
//  summary; otherwise the run has stopped with what to do next.
static bool run_starter_rng_in_program(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngPlanned& planned, const StarterRngPlanOptions& options, Language language,
    const StarterRngNotifications& notifications, EventNotificationOption& finished
){
    RngPlanTracker tracker(
        planned.attempts, options.plan_candidates(env.console, planned.tid), options.SHINY_REQUIRED, STARTER_RNG_LIMITS.max_retries
    );
    ConsoleStarterRngSteps steps(
        env, context, StarterRngConsoleSettings{.language = language, .sound = options.SOUND}, notifications
    );
    RngPlanLogger logger(env.console);
    const StarterRngLoopResult result = run_starter_rng_plan(
        logger, tracker, steps, STARTER_RNG_LIMITS, planned.tid, options.helper_candidates(planned.tid)
    );
    const std::string message = starter_rng_loop_message(result);
    if (result.end != StarterRngLoopEnd::SHINY && result.end != StarterRngLoopEnd::SHINY_OFF_TARGET){
        stop_for_handoff(env, finished, message);
        return false;
    }
    env.console.log(message, COLOR_BLUE);
    env.console.overlay().add_log(message);
    return true;
}

//  After a shiny Squirtle: off its summary, then the rival's battle, which
//  saves (00.04b), and the rows after it to `last`. Until that save, a
//  reload would load the save under the ball and lose the shiny, so a
//  failure stops the run instead. Returns the last row's id.
static std::string run_after_shiny_starter(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryEnv& story, StoryExecutor& executor, const Pokemon::SegmentPolicy& policy, size_t ball_row, size_t last
){
    const std::vector<StoryCheckpoint> rows = rows_after_starter_rng(story_checkpoints(), ball_row, last);
    //  As Starter RNG leaves the summary for the overworld.
    pbf_mash_button(context, BUTTON_B, 5000ms);
    context.wait_for_all_requests();
    env.console.log("The shiny Squirtle isn't saved until " + rows.front().id + " saves after the rival's battle. "
        "If it fails before then, the run stops without reloading: save by hand.", COLOR_BLUE);
    story.has_save = false;
    run_story_rows(env, story, executor, rows, policy, nullptr);
    return rows.back().id;
}

StoryHandoffs AutoStory::story_handoffs() const{
    //  Running Starter RNG here splits 00.04 at the ball as its stop does,
    //  and sets the SID at 00.01 when it isn't known.
    return StoryHandoffs{
        .sid_helper = RUN_STARTER_RNG ? static_cast<std::string>(RNG_PLAN.SID).empty() : bool(STOP_FOR_SID_HELPER),
        .starter_rng = STOP_FOR_STARTER_RNG || RUN_STARTER_RNG,
    };
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    const std::string player_name = PLAYER_NAME;
    check_setup(env.console, checkpoints, first, last, NEW_GAME_FROM_TITLE, player_name);
    const StoryHandoffs handoffs = story_handoffs();
    StoryRunPlan plan = plan_story_run(checkpoints, first, last, handoffs);
    if (plan.stop == StoryHandoff::SID_HELPER && !RUN_STARTER_RNG){
        stop_for_sid_helper(env, context, NOTIFICATION_PROGRAM_FINISH, player_name);
        return;
    }
    if (plan.stop != StoryHandoff::NONE){
        //  The options' text, before anything is pressed.
        RNG_PLAN.check(env.console);
    }
    if (plan.stop == StoryHandoff::SID_HELPER){
        set_sid_in_program(env, context, player_name, RNG_PLAN, RUN.language());
        first = checkpoint_index(checkpoints, "00.01b");
        plan = plan_story_run(checkpoints, first, last, handoffs);
    }

    StoryEnv story{env.console, context, checkpoints[first].supplies, checkpoints[first].supplies, false, RUN.checks()};
    story.player_name = player_name;
    prepare_start(env, context, checkpoints, first, story);
    StoryExecutor executor(story);
    executor.set_checks(RUN.checks());
    executor.set_language(RUN.language());

    StarterRngPlanned planned;
    std::function<void()> before;
    if (plan.stop == StoryHandoff::STARTER_RNG){
        before = [&]{ planned = plan_starter_rng_attempts(env, context, RNG_PLAN, RUN.language()); };
    }
    run_story_rows(env, story, executor, plan.rows, RUN.policy(), std::move(before));
    std::string done_through = plan.rows.back().id;
    if (plan.stop == StoryHandoff::STARTER_RNG){
        std::optional<std::string> done = after_starter_ball(
            env, context, story, executor, planned, checkpoint_index(checkpoints, done_through)
        );
        if (!done){
            return;
        }
        done_through = *done;
    }
    env.console.log("AutoStory: done through " + done_through + ".");
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}

std::optional<std::string> AutoStory::after_starter_ball(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryEnv& story, StoryExecutor& executor, const StarterRngPlanned& planned, size_t ball_row
){
    if (!RUN_STARTER_RNG){
        stop_for_starter_rng(env, NOTIFICATION_PROGRAM_FINISH, RNG_PLAN.sound_name(), rng_attempt_lines(planned.attempts));
        return std::nullopt;
    }
    const StarterRngNotifications notifications{
        .shiny = NOTIFICATION_SHINY, .status_update = NOTIFICATION_STATUS_UPDATE, .error_recoverable = NOTIFICATION_ERROR_RECOVERABLE,
    };
    if (!run_starter_rng_in_program(env, context, planned, RNG_PLAN, RUN.language(), notifications, NOTIFICATION_PROGRAM_FINISH)){
        return std::nullopt;
    }
    return run_after_shiny_starter(env, context, story, executor, RUN.policy(), ball_row, RUN.end_index());
}




}
}
}
