/*  AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <optional>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "Pokemon/AutoStory/Pokemon_StoryPreflight.h"
#include "Pokemon/AutoStory/Pokemon_StoryRun.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_NamingScreenKeyboard.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_TrainerCard.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngConsoleSteps.h"
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

//  The start and end dropdowns' entries: the table's checkpoints, 01.01 to
//  04.03. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const std::vector<std::string> labels = []{
        std::vector<std::string> result;
        for (const StoryCheckpoint& checkpoint : story_checkpoints()){
            result.push_back(Pokemon::story_checkpoint_label(checkpoint));
        }
        return result;
    }();
    static const StringSelectDatabase database = Pokemon::story_checkpoint_database(labels);
    return database;
}

const std::string KEEP_START_FRAME_TEXT =
    "<b>Keep the start frame:</b><br>Off by default. The save loaded was made by hand where the start checkpoint "
    "begins: keep the screen after loading it as that checkpoint's start frame, which later runs compare their "
    "save with. The program keeps one by itself at each save it makes.";

} // namespace

const std::string RUN_STARTER_RNG_TEXT =
    "<b>Run Starter RNG in AutoStory:</b><br>Off by default. Instead of stopping for them, run SID Helper's steps on the "
    "new game's keyboard when the RNG plan has no SID, and Starter RNG's attempts from a save south of the table at the "
    "end of 01.05, as the RNG plan lists them. A shiny Squirtle goes on to the rival's battle, saves after it and the run "
    "goes on; otherwise the run stops with the save south of the table.<br>"
    "SID Helper's timing needs the text speed and button settings already saved, as with Stop for SID Helper.";

const std::string RUN_GIFT_RNG_TEXT =
    "<b>Run Gift RNG in AutoStory:</b><br>Off by default. After 04.02 saves beside the Magikarp salesman in Route 4's "
    "Pokemon Center, run Gift RNG's attempts for the Magikarp from that save, as the RNG plan lists them for its TID and "
    "SID (set by Run Starter RNG in AutoStory, or typed in). A shiny Magikarp is saved there (04.03) and the run goes "
    "on; otherwise the run stops with the save beside the salesman.<br>"
    "To try again from that save, start at 04.03 with this on: it loads the save and runs the attempts first. "
    "After Gift RNG run by hand, start at 04.03 with this off.<br>"
    "English only. The Magikarp costs 500 and needs a free party slot; AutoStory's options leave the text fast.";

AutoStory::AutoStory()
    : RUN(checkpoint_database(), Pokemon::FireRedLeafGreenLanguages_Database())
    , NEW_GAME_FROM_TITLE(
        "<b>New game from the title screen:</b><br>The game is on its title screen with no save data. "
        "Needed to start at 01.01. Otherwise the program loads the save, which must be the one the start checkpoint begins from.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , PLAYER_GENDER(
        "<b>Player:</b><br>Who a new game plays as, picked at \"Are you a boy or a girl?\". "
        "The player's sprite is read as this one's, so a save must be of the same player.",
        {
            {PlayerGender::BOY, "boy", "Boy"},
            {PlayerGender::GIRL, "girl", "Girl"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        PlayerGender::BOY
    )
    , PLAYER_NAME(
        false,
        "<b>Player name:</b><br>Typed on the new game's keyboard (01.01). Empty keeps the game's default name. "
        "The rival always keeps the default name.",
        LockMode::LOCK_WHILE_RUNNING,
        "Kosmos", ""
    )
    , STOP_FOR_SID_HELPER(
        "<b>Stop for SID Helper:</b><br>Off by default. At 01.01, stop on the player's keyboard with the name typed and OK not pressed, "
        "where the SID Helper program starts. After SID Helper, start AutoStory at 01.02, with the game as SID Helper left it "
        "(the Trainer Card, or HOME): it closes the card, sets the options and saves in the bedroom.<br>"
        "SID Helper needs the text speed and button settings already saved. On a cartridge with that save, choose NEW GAME "
        "by hand and start AutoStory on the first message.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , STOP_FOR_STARTER_RNG(
        "<b>Stop for Starter RNG:</b><br>Off by default. At the end of 01.05, save south of the table facing Squirtle's ball "
        "and stop, where the Starter RNG program starts. After Starter RNG, save right after the rival's battle without moving "
        "and start AutoStory at 02.01, listing the targets to try from the RNG plan.<br>"
        "With this off, 01.05 and 01.06 run as one checkpoint, as before.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , RUN_STARTER_RNG(RUN_STARTER_RNG_TEXT, LockMode::LOCK_WHILE_RUNNING, false)
    , RUN_GIFT_RNG(RUN_GIFT_RNG_TEXT, LockMode::LOCK_WHILE_RUNNING, false)
    , NOTIFICATION_SHINY("Shiny found", true, true, ImageAttachmentMode::JPG, {"Notifs", "Showcase"})
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , KEEP_START_FRAME(KEEP_START_FRAME_TEXT, LockMode::LOCK_WHILE_RUNNING, false)
    , NOTIFICATIONS({&NOTIFICATION_SHINY, &NOTIFICATION_STATUS_UPDATE, &NOTIFICATION_PROGRAM_FINISH, &NOTIFICATION_ERROR_FATAL})
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(NEW_GAME_FROM_TITLE);
    PA_ADD_OPTION(PLAYER_GENDER);
    PA_ADD_OPTION(KEEP_START_FRAME);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(STOP_FOR_SID_HELPER);
    PA_ADD_OPTION(STOP_FOR_STARTER_RNG);
    PA_ADD_OPTION(RUN_STARTER_RNG);
    PA_ADD_OPTION(RUN_GIFT_RNG);
    PA_ADD_OPTION(RNG_PLAN);
    PA_ADD_OPTION(NOTIFICATIONS);
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

//  The story's state for a run from `checkpoint`: its supplies, the checks
//  level, and the program's start frames, the first start check keeping
//  the screen as its frame if `keep_start_frame` (a save made by hand).
StoryEnv story_env(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StoryCheckpoint& checkpoint, Pokemon::StoryCheckLevel checks, bool keep_start_frame
){
    StoryEnv story{env.console, context, checkpoint.supplies, checkpoint.supplies, false, checks};
    story.start_frames = program_start_frames();
    story.keep_start_frame = keep_start_frame;
    return story;
}

//  The start gate's preflight failed: the console's setup, nothing pressed.
void throw_if_not_started(ConsoleHandle& console, const Pokemon::Outcome& outcome){
    if (outcome.failure == Pokemon::StoryPreflight::FAILURE){
        throw UserSetupError(console, "AutoStory did not start, nothing was pressed: " + outcome.detail);
    }
}

//  The start gate's preflight on its own, for the steps that press before
//  the start checkpoint is reached (SID Helper's name, the RNG plan's TID).
void require_preflight(ConsoleHandle& console, ProControllerContext& context){
    throw_if_not_started(console, Pokemon::StoryPreflight::read(console, context).check());
}

//  What FRLG lends the shared shell: the console's preflight; a load is a soft reset, checked
//  against the save card for the map the checkpoint under way starts on,
//  then the supplies the save had, read from its bag again; the engine's
//  lines go to the console's log.
Pokemon::StoryGameHooks story_game(SingleSwitchProgramEnvironment& env, StoryEnv& story){
    Pokemon::StoryGameHooks game;
    game.preflight = [&story]{
        return Pokemon::StoryPreflight::read(story.console, story.context);
    };
    game.load_save = [&story](Pokemon::RunState&){
        if (!story.has_save){
            return Pokemon::outcome_failed("no_save", "nothing saved yet");
        }
        try{
            soft_reset(story.console, story.context);
        }catch (OperationFailedException& e){
            return Pokemon::outcome_failed("reload_failed", e.message());
        }
        return Pokemon::outcome_ok();
    };
    //  The save reloaded must be where the checkpoint starts: a save that
    //  didn't happen leaves an older one (console B, 2026-11-02: 01.03
    //  walked the bedroom's route in Pewter City).
    game.check_save_place = [&story]{
        return check_save_place(story, story.start_map);
    };
    game.after_load = [&story](Pokemon::RunState&){
        story.supplies = story.saved_supplies;
        //  Until read in full, so each verify run starts from the bag as
        //  the save holds it, not from a count once read wrong or not at
        //  all (console B, 2026-11-03: 0 Potions in all 3 runs). Once
        //  read, a reload of the same save restores it.
        try{
            if (!story.bag_read){
                read_bag_supplies(story, story.start_menu);
            }
        }catch (OperationFailedException& e){
            return Pokemon::outcome_failed("reload_failed", e.message());
        }
        return Pokemon::outcome_ok();
    };
    game.log = [&env](const std::string& line){
        env.console.log(line);
    };
    //  The checkpoint and the step under way, for whoever watches the stream.
    game.status = [&env](const std::string& line){
        env.console.overlay().add_log(line);
    };
    return game;
}

//  Loads the save checkpoint `checkpoint` starts from through the story run,
//  which checks it is where the checkpoint starts (console B, 2026-11-01:
//  03.02 on the forest's save), then reads the lead's moves.
void load_start_save(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, Pokemon::StoryGameHooks& game,
    Pokemon::RunState& state, const StoryCheckpoint& checkpoint
){
    story.has_save = true;
    story.start_map = Pokemon::grid_pose_of(*checkpoint.start).map;
    story.start_menu = checkpoint.menu;
    const Pokemon::Outcome loaded = Pokemon::prepare_story_start(checkpoint.id, Pokemon::StoryStart::FROM_SAVE, game, state);
    throw_if_not_started(env.console, loaded);
    if (loaded.failure == "wrong_save"){
        throw UserSetupError(env.console, loaded.detail + " Start at the checkpoint " + checkpoint.id + " the save is for.");
    }
    if (!loaded.succeeded()){
        Pokemon::log_story_failure(game);
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT, "AutoStory could not load the save for " + checkpoint.id + ": " + Pokemon::to_string(loaded)
        );
    }
    read_lead_moves(story, checkpoint.menu);
}

//  The game on the save checkpoint `checkpoint` starts from, with the
//  supplies and moves read from it.
void load_verify_save(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, Pokemon::StoryGameHooks& game,
    Pokemon::RunState& state, const StoryCheckpoint& checkpoint
){
    if (!checkpoint.start){
        throw UserSetupError(env.console,
            "Verify mode reloads the checkpoint's save between runs: " + checkpoint.id + " starts a new game and has none."
        );
    }
    if (checkpoint.live_start){
        throw UserSetupError(env.console,
            "Verify mode reloads the checkpoint's save between runs: " + checkpoint.id + " starts from the game as it is, with no save."
        );
    }
    load_start_save(env, story, game, state, checkpoint);
}

//  Verify mode: the start checkpoint run from its save as `policy` says,
//  never saving at its end, so every run starts from the same save.
//  Throws unless enough runs pass.
void verify_story_checkpoint(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const Pokemon::StoryRunOptions& run, const Pokemon::VerifyPolicy& policy, EventNotificationOption& finished
){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const StoryCheckpoint& checkpoint = checkpoints[run.start_index()];
    StoryEnv story = story_env(env, context, checkpoint, run.check_level(), false);
    story.language = run.language();
    Pokemon::StoryGameHooks game = story_game(env, story);
    Pokemon::RunState state;
    load_verify_save(env, story, game, state, checkpoint);
    StoryExecutor executor(story);
    Pokemon::Segment segment = make_story_segment(checkpoints, executor);
    Pokemon::SegmentHooks hooks = Pokemon::story_segment_hooks(game);

    Pokemon::VerifyReport report;
    try{
        report = Pokemon::verify_checkpoint(checkpoint.id, policy, segment, hooks, state);
    }catch (OperationFailedException&){
        Pokemon::log_story_failure(game);
        throw;
    }
    const std::string summary = Pokemon::finish_story_verify(checkpoint.id, policy, report, game);
    send_program_finished_notification(env, finished, summary);
}


} // namespace

//  The options' problems, before anything is pressed.
static void check_setup(
    ConsoleHandle& console, const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last,
    bool new_game_from_title, const std::string& player_name
){
    if (first > last || last >= checkpoints.size()){
        throw UserSetupError(console, "The start checkpoint must not come after the end checkpoint.");
    }
    if (first == 0 && !new_game_from_title){
        throw UserSetupError(console, "Checkpoint 01.01 starts a new game: start on the title screen with no save.");
    }
    if (first == 0){
        check_player_name(console, player_name);
    }
}

//  Gets the game to where checkpoint `first` starts: a new game from the
//  title screen, the game as another program left it, or the checkpoint's
//  save loaded. `at_handoff`: the run starts with the RNG handoff before a
//  live-start row, whose save is where that row starts, so it is loaded.
static void prepare_start(
    SingleSwitchProgramEnvironment& env,
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, StoryEnv& story, bool at_handoff
){
    const StoryCheckpoint& checkpoint = checkpoints[first];
    Pokemon::StoryGameHooks game = story_game(env, story);
    Pokemon::RunState state;
    if (!checkpoint.start || (checkpoint.live_start && !at_handoff)){
        const Pokemon::StoryStart start = checkpoint.start ? Pokemon::StoryStart::LIVE : Pokemon::StoryStart::NEW_GAME;
        throw_if_not_started(env.console, Pokemon::prepare_story_start(checkpoint.id, start, game, state));
        return;
    }
    load_start_save(env, story, game, state, checkpoint);
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
    EventNotificationOption& notification, const std::string& player_name, PlayerGender gender
){
    play_new_game_to_player_name(env.console, context, player_name, gender);
    stop_for_handoff(env, notification,
        "Stopped for SID Helper: the player's name is typed and OK is not pressed. Run SID Helper now, "
        "enter its Target Advances and # Candidate SIDs in the RNG plan, then start AutoStory at 01.02."
    );
}

//  The TID for the RNG plan: the RNG plan's, or read off the Trainer Card
//  (START, the player's entry), which is then closed again. Run before
//  `checkpoint` saves, which a failure tells the user to start at again.
static uint16_t rng_plan_tid(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngPlanOptions& options, Language language, const std::string& checkpoint
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
            "Couldn't read the TID on the Trainer Card, so there is no RNG plan. Nothing was saved: "
            "type the TID into the RNG plan's TID option and start AutoStory at " + checkpoint + " again.",
            env.console
        );
    }
    close_trainer_card(env.console, context);
    env.console.log(rng_log_tid(*tid, true));
    return *tid;
}

//  The RNG plan's attempts for the handoff `stop` (STARTER_RNG or
//  GIFT_RNG) that ends `checkpoint`. Run where it ends, before it saves
//  there, so a failure ends the run with the save it started from kept.
static StarterRngPlanned plan_handoff_attempts(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngPlanOptions& options, Language language, StoryHandoff stop, const std::string& checkpoint
){
    const uint16_t tid = rng_plan_tid(env, context, options, language, checkpoint);
    try{
        std::vector<RngAttempt> attempts = stop == StoryHandoff::GIFT_RNG
            ? options.gift_plan(env.console, language, tid)
            : options.plan(env.console, language, tid);
        return StarterRngPlanned{.tid = tid, .attempts = std::move(attempts)};
    }catch (UserSetupError& e){
        throw UserSetupError(env.console, e.message() + " Nothing was saved: fix the RNG plan and start AutoStory at " + checkpoint + " again.");
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
        "Then save right after the rival's battle, without moving, and start AutoStory at 02.01.",
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
    Pokemon::StoryGameHooks game = story_game(env, story);
    Pokemon::SegmentHooks hooks = Pokemon::story_segment_hooks(game);

    Pokemon::RunState state;
    Pokemon::SegmentReport report;
    try{
        report = run_segment(Pokemon::SegmentRange{rows.front().id, rows.back().id}, policy, segment, hooks, state);
    }catch (OperationFailedException&){
        Pokemon::log_story_failure(game);
        throw;
    }
    try{
        Pokemon::finish_story_run(report, rows.back().id, env.current_stats<Pokemon::AutoStoryStats>(), game);
    }catch (OperationFailedException&){
        env.update_stats();
        throw;
    }
    env.update_stats();
}

//  SID Helper's steps in the program, from the player's keyboard: the name
//  typed, then the intro at SID Helper's timing into the bedroom, and the
//  Trainer Card opened to read the TID. Ends on the card, where 01.02
//  starts.
static void set_sid_in_program(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& player_name, PlayerGender gender, const StarterRngPlanOptions& options, Language language
){
    const uint32_t target_advances = options.SID_HELPER_ADVANCES;
    if (target_advances % 2 == 0){
        throw UserSetupError(env.console, "RNG plan: SID Helper's Target Advances must be odd.");
    }
    play_new_game_to_player_name(env.console, context, player_name, gender);
    const SidTiming timing = sid_timing(language, target_advances);
    env.log("SID Helper's steps: delay " + std::to_string(timing.delay_ms) + "ms");
    set_sid_from_name_screen(context, timing);
    finish_intro_animations(env, context);

    navigate_to_trainer_card(env.console, context);
    const std::optional<uint16_t> tid = try_read_trainer_card_tid(env.console, context, language);
    if (!tid){
        env.console.log("Couldn't read the TID on the Trainer Card; 01.05 reads it again.", COLOR_ORANGE);
        return;
    }
    env.console.log(rng_log_tid(*tid, true));
    env.console.log(rng_log_sid_helper(*tid, target_advances, options.helper_candidates(*tid)));
}

namespace{

//  Run Starter RNG and Gift RNG in AutoStory: the time caps, both programs'
//  default Max Resets per try, and an attempt's misses before the run stops.
const StarterRngLoopLimits RNG_LOOP_LIMITS{
    .attempt_time = std::chrono::hours(3),
    .total_time = std::chrono::hours(12),
    .max_resets = 50,
    .max_retries = 2,
};

} // namespace

//  The RNG plan's attempts for `planned`, each tried through `steps`. A
//  shiny that confirms the SID writes it into the RNG plan's SID option.
static StarterRngLoopResult run_rng_plan_in_program(
    SingleSwitchProgramEnvironment& env, StarterRngLoopSteps& steps,
    const StarterRngPlanned& planned, StarterRngPlanOptions& options
){
    RngPlanTracker tracker(
        planned.attempts, options.plan_candidates(env.console, planned.tid), options.SHINY_REQUIRED, RNG_LOOP_LIMITS.max_retries
    );
    RngPlanLogger logger(env.console);
    const StarterRngLoopResult result = run_starter_rng_plan(
        logger, tracker, steps, RNG_LOOP_LIMITS, planned.tid, options.helper_candidates(planned.tid)
    );
    const std::string confirmed_sid = result.sid ? std::to_string(*result.sid) : "";
    if (result.end == StarterRngLoopEnd::SHINY && result.sid && static_cast<std::string>(options.SID) != confirmed_sid){
        //  Kept in AutoStory's settings, so the next run plans for it and skips SID
        //  Helper's steps. Deferred: a per-save record keyed by TID (e.g. UserSettings/
        //  FRLG-Saves.json: TID -> SID, version, language) that any FRLG RNG program
        //  could read, once a second program needs the SID.
        options.SID.set(confirmed_sid);
        env.console.log(
            "AutoStory: set the RNG plan's SID to " + confirmed_sid + ". The settings file gets it "
            "when the program's panel is closed or the app quits.", COLOR_BLUE
        );
    }
    return result;
}

//  True for a shiny, on the target or not, with `message` logged; otherwise
//  the run has stopped with it.
static bool shiny_or_stop(
    SingleSwitchProgramEnvironment& env, EventNotificationOption& finished,
    const StarterRngLoopResult& result, const std::string& message
){
    if (result.end != StarterRngLoopEnd::SHINY && result.end != StarterRngLoopEnd::SHINY_OFF_TARGET){
        stop_for_handoff(env, finished, message);
        return false;
    }
    env.console.log(message, COLOR_BLUE);
    env.console.overlay().add_log(message);
    return true;
}

//  Starter RNG's attempts in the program, from the save south of the table.
//  Returns true for a shiny Squirtle, the game on its summary; otherwise
//  the run has stopped with what to do next.
static bool run_starter_rng_in_program(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngPlanned& planned, StarterRngPlanOptions& options, Language language,
    const StarterRngNotifications& notifications, EventNotificationOption& finished
){
    //  The settings file gets the calibration saved after each reset when
    //  the program's panel is closed or the app quits.
    RngStopCalibrationOptions& calibration = options.CALIBRATION;
    ConsoleStarterRngSteps console_steps(env, context, StarterRngConsoleSettings{
        .language = language, .sound = options.SOUND, .calibration = calibration.calibration(calibration.STARTER_INGAME),
    }, notifications);
    CalibrationSavingSteps steps(console_steps, calibration, calibration.STARTER_INGAME);
    const StarterRngLoopResult result = run_rng_plan_in_program(env, steps, planned, options);
    return shiny_or_stop(env, finished, result, starter_rng_loop_message(result));
}

//  Gift RNG's attempts in the program, from the save beside the Magikarp
//  salesman. Returns true for a shiny Magikarp, the game on its summary;
//  otherwise the run has stopped with what to do next.
static bool run_gift_rng_in_program(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngPlanned& planned, StarterRngPlanOptions& options, Language language,
    const GiftRngNotifications& notifications, EventNotificationOption& finished
){
    //  The console's seed and CSF offsets, from Starter RNG too, and Gift
    //  RNG's own in-game offset, saved as Starter RNG's are.
    RngStopCalibrationOptions& calibration = options.CALIBRATION;
    ConsoleGiftRngSteps console_steps(env, context, GiftRngConsoleSettings{
        .language = language, .sound = options.SOUND, .calibration = calibration.calibration(calibration.GIFT_INGAME),
    }, notifications);
    CalibrationSavingSteps steps(console_steps, calibration, calibration.GIFT_INGAME);
    const StarterRngLoopResult result = run_rng_plan_in_program(env, steps, planned, options);
    return shiny_or_stop(env, finished, result, gift_rng_loop_message(result));
}

//  After a shiny Squirtle: off its summary, and the rows to run next: the
//  rival's battle, which saves (01.06), and the rows after it to `last`.
//  Until that save, a reload would load the save under the ball and lose
//  the shiny, so a failure stops the run instead.
static StoryRunPlan after_shiny_starter(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context, StoryEnv& story,
    size_t ball_row, size_t last, const StoryHandoffs& handoffs
){
    StoryRunPlan plan = rows_after_starter_rng(story_checkpoints(), ball_row, last, handoffs);
    //  As Starter RNG leaves the summary for the overworld.
    pbf_mash_button(context, BUTTON_B, 5000ms);
    context.wait_for_all_requests();
    env.console.log("The shiny Squirtle isn't saved until " + plan.rows.front().id + " saves after the rival's battle. "
        "If it fails before then, the run stops without reloading: save by hand.", COLOR_BLUE);
    story.has_save = false;
    return plan;
}

//  After a shiny Magikarp: the rows to run next, from 04.03, which leaves
//  its summary and saves, to `last`. Until that save, a failure stops the
//  run, as after a shiny Squirtle.
static StoryRunPlan after_shiny_magikarp(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, size_t salesman_row, size_t last, const StoryHandoffs& handoffs
){
    StoryRunPlan plan = rows_after_gift_rng(story_checkpoints(), salesman_row, last, handoffs);
    env.console.log("The shiny Magikarp isn't saved until " + plan.rows.front().id + " saves. "
        "If it fails before then, the run stops without reloading: save by hand.", COLOR_BLUE);
    story.has_save = false;
    return plan;
}

//  The run is done through `done_through`. The finish message also gives
//  the SID when an RNG loop set it, from `sid_before` to `sid_after`.
static void send_story_finished(
    SingleSwitchProgramEnvironment& env, EventNotificationOption& finished,
    const std::string& done_through, const std::string& sid_before, const std::string& sid_after
){
    std::string message = "AutoStory: done through " + done_through + ".";
    if (sid_after != sid_before){
        message += " The RNG plan's attempts confirmed the SID, " + sid_after + ", and saved it as the RNG plan's SID.";
    }
    env.console.log(message);
    send_program_finished_notification(env, finished, message);
}

StoryHandoffs AutoStory::story_handoffs() const{
    //  Running Starter RNG here splits 01.05 at the ball as its stop does,
    //  and sets the SID at 01.01 when it isn't known.
    return StoryHandoffs{
        .sid_helper = RUN_STARTER_RNG ? static_cast<std::string>(RNG_PLAN.SID).empty() : bool(STOP_FOR_SID_HELPER),
        .starter_rng = STOP_FOR_STARTER_RNG || RUN_STARTER_RNG,
        .gift_rng = RUN_GIFT_RNG,
    };
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    if (const std::optional<Pokemon::VerifyPolicy> verify = RUN.VERIFY.policy()){
        verify_story_checkpoint(env, context, RUN, *verify, NOTIFICATION_PROGRAM_FINISH);
        return;
    }
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    const std::string player_name = PLAYER_NAME;
    check_setup(env.console, checkpoints, first, last, NEW_GAME_FROM_TITLE, player_name);
    require_preflight(env.console, context);
    const StoryHandoffs handoffs = story_handoffs();
    const std::string sid_before = RNG_PLAN.SID;
    StoryRunPlan plan = plan_story_run(checkpoints, first, last, handoffs);
    if (plan.stop == StoryHandoff::SID_HELPER && !RUN_STARTER_RNG){
        stop_for_sid_helper(env, context, NOTIFICATION_PROGRAM_FINISH, player_name, PLAYER_GENDER);
        return;
    }
    if (plan.stop != StoryHandoff::NONE){
        //  The options' text, before anything is pressed.
        RNG_PLAN.check(env.console);
    }
    if (plan.stop == StoryHandoff::SID_HELPER){
        set_sid_in_program(env, context, player_name, PLAYER_GENDER, RNG_PLAN, RUN.language());
        first = checkpoint_index(checkpoints, "01.02");
        plan = plan_story_run(checkpoints, first, last, handoffs);
    }
    StoryEnv story = story_env(env, context, checkpoints[first], RUN.check_level(), KEEP_START_FRAME);
    story.player_gender = PLAYER_GENDER;
    story.player_name = player_name;
    const bool at_gift_rng = plan.stop == StoryHandoff::GIFT_RNG && plan.rows.empty();
    prepare_start(env, checkpoints, first, story, at_gift_rng);
    story.language = RUN.language();
    StoryExecutor executor(story);

    const std::optional<std::string> done_through = at_gift_rng
        ? run_from_gift_rng_save(env, context, story, executor, first)
        : run_story_plan(env, context, story, executor, std::move(plan));
    if (done_through){
        send_story_finished(env, NOTIFICATION_PROGRAM_FINISH, *done_through, sid_before, RNG_PLAN.SID);
    }
}

std::optional<std::string> AutoStory::run_story_plan(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryEnv& story, StoryExecutor& executor, StoryRunPlan plan
){
    while (true){
        const std::string last_id = plan.rows.back().id;
        const StoryHandoff stop = plan.stop;
        StarterRngPlanned planned;
        std::function<void()> before;
        if (stop == StoryHandoff::STARTER_RNG || stop == StoryHandoff::GIFT_RNG){
            before = [&, stop, last_id]{
                planned = plan_handoff_attempts(env, context, RNG_PLAN, RUN.language(), stop, last_id);
            };
        }
        run_story_rows(env, story, executor, plan.rows, RUN.policy(), std::move(before));
        const size_t row = checkpoint_index(story_checkpoints(), last_id);
        std::optional<StoryRunPlan> next;
        switch (stop){
        case StoryHandoff::NONE:
            return last_id;
        case StoryHandoff::SID_HELPER:
            throw InternalProgramError(&env.logger(), PA_CURRENT_FUNCTION, "SID Helper's stop has no rows to run.");
        case StoryHandoff::STARTER_RNG:
            next = after_starter_ball(env, context, story, planned, row);
            break;
        case StoryHandoff::GIFT_RNG:
            next = after_magikarp_salesman(env, context, story, planned, row);
            break;
        }
        if (!next){
            return std::nullopt;
        }
        plan = std::move(*next);
    }
}

std::optional<std::string> AutoStory::run_from_gift_rng_save(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryEnv& story, StoryExecutor& executor, size_t first
){
    const StarterRngPlanned planned = plan_handoff_attempts(
        env, context, RNG_PLAN, RUN.language(), StoryHandoff::GIFT_RNG, story_checkpoints()[first].id
    );
    std::optional<StoryRunPlan> next = after_magikarp_salesman(env, context, story, planned, first - 1);
    if (!next){
        return std::nullopt;
    }
    return run_story_plan(env, context, story, executor, std::move(*next));
}

std::optional<StoryRunPlan> AutoStory::after_starter_ball(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryEnv& story, const StarterRngPlanned& planned, size_t ball_row
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
    return after_shiny_starter(env, context, story, ball_row, RUN.end_index(), story_handoffs());
}

std::optional<StoryRunPlan> AutoStory::after_magikarp_salesman(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryEnv& story, const StarterRngPlanned& planned, size_t salesman_row
){
    const GiftRngNotifications notifications{
        .shiny = NOTIFICATION_SHINY, .status_update = NOTIFICATION_STATUS_UPDATE, .error_recoverable = NOTIFICATION_ERROR_RECOVERABLE,
    };
    if (!run_gift_rng_in_program(env, context, planned, RNG_PLAN, RUN.language(), notifications, NOTIFICATION_PROGRAM_FINISH)){
        return std::nullopt;
    }
    return after_shiny_magikarp(env, story, salesman_row, RUN.end_index(), story_handoffs());
}




} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
