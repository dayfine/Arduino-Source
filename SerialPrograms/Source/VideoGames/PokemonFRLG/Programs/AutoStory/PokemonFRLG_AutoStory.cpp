/*  AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/StaticGlobals.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StorySavePolicy.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryVerifyRuns.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_OverlayLog.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Pokemon_StoryPreflight.h"
#include "Pokemon/AutoStory/Pokemon_StoryRngHandoff.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStart.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_NamingScreenKeyboard.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_TrainerCard.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_GiftRngConsoleSteps.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanLog.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTracker.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SidSteps.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngConsoleSteps.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngLoop.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFailureClasses.h"
#include "PokemonFRLG_AutoStoryLaunch.h"
#include "PokemonFRLG_AutoStoryLeagueSteps.h"
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

constexpr std::string_view STORY_LEFT_WHERE_FAILED = "AutoStory failed: the game stays where it failed.";

//  Thin console StoryBackend for FRLG (E10): tags failure lines, forwards
//  short status to the video overlay, exposes CancellableScope.
class FrlgConsoleBackend final : public Pokemon::StoryBackend{
public:
    FrlgConsoleBackend(SingleSwitchProgramEnvironment& env, ProControllerContext& context)
        : m_env(env)
        , m_context(context)
    {}

    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        pbf_press_button(m_context, button, hold, release);
    }
    virtual void stick(double, Milliseconds, Milliseconds, Pokemon::StoryPace) override{}
    virtual void wait(Milliseconds duration) override{
        m_context.wait_for(duration);
    }
    virtual ScreenObservation observe() override{
        return ScreenObservation{};
    }
    virtual void log(std::string_view line) override{
        const std::string text(line);
        m_env.console.log(tag_failure_line(text));
        Pokemon::add_wrapped_overlay_log(m_env.console.overlay(), Pokemon::story_short_line(text), COLOR_WHITE);
    }
    virtual void log_detail(std::string_view line) override{
        m_env.console.log(tag_failure_line(std::string(line)));
    }
    virtual CancellableScope& scope() override{
        return m_context;
    }

private:
    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
};

const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        StringSelectDatabase db;
        const std::vector<StoryCheckpoint> rows = frlg_rows_for_mode(load_frlg_story({}), STATIC_GLOBALS.DEVELOPER_MODE);
        for (size_t i = 0; i < rows.size(); i++){
            db.add_entry(StringSelectEntry(
                rows[i].id,
                Pokemon::story_checkpoint_label(rows[i].id, rows[i].title)
            ));
        }
        return db;
    }();
    return database;
}

const std::string PLAYER_NAME_TEXT =
    "<b>Player name:</b><br>Typed on the new game's keyboard (01.01). Empty keeps the game's default name. "
    "The rival always keeps the default name.";

const std::string FIELD_MOVE_USERS_TEXT =
    "<b>Field move users:</b><br>Set by the run at each save: the party slot and species that use each field move "
    "(e.g. fly=3:pidgey), so a Fly after the program restarts goes to that slot without reading the party's summaries. "
    "A slot that holds another member now, or an entry that can't be read, only costs a party scan. "
    "Clear it when loading a different save.";

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
    "on; otherwise the run stops with the save beside the salesman. Likewise for the Eevee: 09.03 saves facing its ball "
    "in Celadon Mansion's roof room, and 09.04 saves the shiny; and for the Lapras: 16.03b saves facing the employee "
    "on Silph Co.'s 7F, and 16.04 saves the shiny.<br>"
    "To try again from that save, start at 04.03 (09.04, 16.04) with this on: it loads the save and runs the attempts first. "
    "After Gift RNG run by hand, start at 04.03 (09.04, 16.04) with this off.<br>"
    "English only. The Magikarp costs 500; each gift needs a free party slot; AutoStory's options leave the text fast.";

const std::string TAKE_MASTER_BALL_TEXT =
    "<b>Take the Master Ball:</b><br>On by default. After Giovanni on Silph Co.'s 11F, talk to the president for the "
    "Master Ball (16.05b). Off, or with the checks on SPEEDRUN, skips it.";

const std::string NUGGET_FARM_COUNT_TEXT =
    "<b>Nugget Bridge farm count:</b><br>How many Nuggets to farm by losing to the Nugget Bridge Rocket (05.04). "
    "0 skips farming and the lead PC swap.";

// code-health: allow function_lines -- a flat list of the program's UI options registered in constructor order
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
    , PLAYER_NAME(false, PLAYER_NAME_TEXT, LockMode::LOCK_WHILE_RUNNING, "Kosmos", "")
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
    , TAKE_MASTER_BALL(TAKE_MASTER_BALL_TEXT, LockMode::LOCK_WHILE_RUNNING, true)
    , NUGGET_FARM_COUNT(NUGGET_FARM_COUNT_TEXT, LockMode::LOCK_WHILE_RUNNING, 10, 0, 99)
    , FIELD_MOVE_USERS(false, FIELD_MOVE_USERS_TEXT, LockMode::LOCK_WHILE_RUNNING, "", "fly=3:pidgey")
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
    PA_ADD_OPTION(TAKE_MASTER_BALL);
    PA_ADD_OPTION(NUGGET_FARM_COUNT);
    PA_ADD_OPTION(RNG_PLAN);
    PA_ADD_OPTION(FIELD_MOVE_USERS);
    PA_ADD_OPTION(NOTIFICATIONS);
}


namespace{

void check_player_name(ConsoleHandle& console, const std::string& name){
    if (name.empty()){
        return;
    }
    const std::string error = naming_text_error(name, NamingScreenKind::PLAYER);
    if (!error.empty()){
        throw UserSetupError(console, "Player name: " + error);
    }
}

StoryEnv story_env(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StoryCheckpoint& checkpoint, Pokemon::StoryCheckLevel checks, bool keep_start_frame
){
    StoryEnv story{env.console, context, checkpoint.supplies, checkpoint.supplies, false, {}, checks};
    story.start_frames = program_start_frames();
    story.keep_start_frame = keep_start_frame;
    return story;
}

void keep_field_move_users(ConsoleHandle& console, StoryEnv& story, StringOption& setting){
    const ParsedFieldMoveUsers parsed = parse_field_move_users(static_cast<std::string>(setting));
    for (const std::string& entry : parsed.rejected){
        console.log(std::format("AutoStory: Field move users: can't read \"{}\"; the party is scanned for it.", entry), COLOR_ORANGE);
    }
    story.party.users() = parsed.users;
    story.saved_field_moves = SavedFieldMoveUsers(parsed.users, [&console, &setting](const FieldMoveUsers& users){
        const std::string text = users.to_text();
        if (static_cast<std::string>(setting) != text){
            setting.set(text);
            console.log(std::format("AutoStory: Field move users set to \"{}\".", text), COLOR_BLUE);
        }
    });
}

void set_breakpoints(SingleSwitchProgramEnvironment& env, ProControllerContext& context, StoryEnv& story, Pokemon::StoryBreakpoints mode){
    story.pause = Pokemon::StoryPausePoints(mode, Pokemon::make_story_pause_hook(context));
    story.pause.set_log([&env](std::string_view line){
        env.console.log(std::string(line));
    });
}

void throw_if_preflight_failed(ConsoleHandle& console, const std::optional<Pokemon::StoryFailure>& failure){
    if (failure && failure->code == Pokemon::StoryPreflight::FAILURE){
        throw UserSetupError(console, "AutoStory did not start, nothing was pressed: " + failure->detail);
    }
}

void require_preflight(ConsoleHandle& console, ProControllerContext& context){
    throw_if_preflight_failed(console, Pokemon::StoryPreflight::read(console, context).check());
}

//  Reloads the current save, checks save place, restores supplies/party, and
//  re-reads the bag once per save. Returns nullopt on success.
std::optional<Pokemon::StoryFailure> reload_frlg_save(SingleSwitchProgramEnvironment& env, StoryEnv& story){
    if (!story.has_save){
        return frlg_failure("no_save", "nothing saved yet", true);
    }
    env.console.log("[Story] loading the save");
    if (std::optional<Pokemon::StoryFailure> failed = launch_frlg_save(story.console, story.context)){
        return failed;
    }
    const std::string place_error = check_save_place(story, story.start_map);
    if (!place_error.empty()){
        return frlg_failure("wrong_save", place_error, true);
    }
    story.run_state = FrlgRunState{};
    story.supplies = story.saved_supplies;
    story.supplies.counts_read = false;
    story.party.invalidate_species();
    story.party.users() = story.saved_field_moves.users();
    try{
        if (!story.bag_read){
            read_bag_supplies(story, story.start_menu);
        }
    }catch (OperationFailedException& e){
        return frlg_failure("reload_failed", e.message(), true);
    }
    return std::nullopt;
}

void load_start_save(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, const StoryCheckpoint& checkpoint
){
    story.has_save = true;
    story.saved_checkpoint_id = checkpoint.id;
    story.start_map = checkpoint.start->map;
    story.start_menu = checkpoint.menu;
    require_preflight(env.console, story.context);
    if (std::optional<Pokemon::StoryFailure> failed = reload_frlg_save(env, story)){
        if (failed->code == "wrong_save"){
            throw UserSetupError(env.console, failed->detail + " Start at the checkpoint " + checkpoint.id + " the save is for.");
        }
        env.console.log(std::string(STORY_LEFT_WHERE_FAILED));
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("AutoStory could not load the save for {}: {}", checkpoint.id, *failed)
        );
    }
    story.run_state.flags.insert("save_loaded");
    read_lead_moves(story, checkpoint.menu);
}

void load_verify_save(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, const StoryCheckpoint& checkpoint
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
    load_start_save(env, story, checkpoint);
}

std::vector<StoryCheckpoint> load_run_rows(ConsoleHandle& console){
    const std::string folder = Pokemon::story_dev_folder("frlg");
    try{
        const FrlgStoryData story = load_frlg_story(Pokemon::read_story_dev_files(folder, "frlg"));
        for (const auto& [name, path] : story.dev_files){
            console.log(std::format("AutoStory: using dev checkpoint {} from {}", name, path));
        }
        return frlg_rows_for_mode(story, STATIC_GLOBALS.DEVELOPER_MODE);
    }catch (const ParseException& error){
        throw UserSetupError(console, std::format("AutoStory: bad dev checkpoint in {}: {}", folder, error.message()));
    }
}

//  Builds #648 StoryGame for FRLG (E6 phase-gated + Hall of Fame save policy,
//  S6 reload returning the checkpoint id whose start save was loaded).
Pokemon::StoryGame make_frlg_story_game(
    SingleSwitchProgramEnvironment& env,
    StoryEnv& story,
    StoryExecutor& executor,
    const std::vector<StoryCheckpoint>& rows,
    const Pokemon::StoryRunPolicy& policy,
    std::function<void()> before_last_save
){
    Pokemon::PhaseGateConfig gate;
    gate.requested_mode = policy.save_mode;
    gate.last_checkpoint_id = rows.empty() ? std::string() : rows.back().id;
    gate.ignore_phase = true;
    for (const StoryCheckpoint& row : rows){
        gate.status_by_id.emplace(row.id, row.phase);
    }

    Pokemon::StorySavePolicy inner_save = frlg_save_policy(story, rows, executor, story.run_state);
    if (before_last_save){
        std::string last_id = gate.last_checkpoint_id;
        inner_save = [save = std::move(inner_save), before = std::move(before_last_save), last_id = std::move(last_id)](
            Pokemon::StoryBackend& backend, const Pokemon::StoryCheckpoint& passed
        ) mutable -> std::optional<Pokemon::StoryFailure>{
            if (passed.id == last_id){
                before();
            }
            return save(backend, passed);
        };
    }

    Pokemon::StoryGame game;
    game.save = Pokemon::phase_gated(std::move(inner_save), std::move(gate));
    game.reload = [&env, &story](
        Pokemon::StoryBackend& backend, const Pokemon::StoryCheckpoint& failed
    ) -> std::optional<std::string>{
        if (!story.has_save || story.saved_checkpoint_id != failed.id){
            return std::nullopt;
        }
        if (std::optional<Pokemon::StoryFailure> err = reload_frlg_save(env, story)){
            backend.log(std::format("[Story] reload failed: {}", *err));
            return std::nullopt;
        }
        return story.saved_checkpoint_id;
    };
    game.attempts_per_checkpoint = policy.attempts_per_checkpoint;
    return game;
}

void verify_story_checkpoint(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const Pokemon::StoryRunOptions& run, const Pokemon::VerifyPolicy& policy, EventNotificationOption& finished
){
    const std::vector<StoryCheckpoint> checkpoints = load_run_rows(env.console);
    const StoryCheckpoint& checkpoint = checkpoints[run.start_index()];
    StoryEnv story = story_env(env, context, checkpoint, run.check_level(), false);
    story.language = run.language();
    set_breakpoints(env, context, story, run.breakpoints());
    load_verify_save(env, story, checkpoint);
    StoryExecutor executor(story);
    executor.set_pause_points(&story.pause);

    FrlgConsoleBackend backend(env, context);
    const std::vector<StoryCheckpoint> one_row{checkpoint};
    Pokemon::StoryGame game = make_frlg_story_game(env, story, executor, one_row, run.policy(), {});
    Pokemon::RunState core_state;

    Pokemon::VerifyModeReport report;
    try{
        report = Pokemon::verify_checkpoint(
            backend,
            checkpoint.id,
            Pokemon::VerifyModePolicy{
                .runs = policy.runs,
                .passes_needed = policy.passes_needed,
                .save_mode = policy.save_mode,
            },
            [&checkpoint, &executor, &story]{
                return make_frlg_engine_checkpoint(checkpoint, executor, story.run_state);
            },
            game,
            core_state
        );
    }catch (OperationFailedException&){
        env.console.log(std::string(STORY_LEFT_WHERE_FAILED));
        throw;
    }
    const std::string summary = std::format(
        "AutoStory verify: {} {}/{} passed, {} needed.",
        checkpoint.id, report.passes, policy.runs, policy.passes_needed
    );
    if (!report.verified){
        env.console.log(std::string(STORY_LEFT_WHERE_FAILED));
        throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, summary);
    }
    env.console.log(summary);
    send_program_finished_notification(env, finished, summary);
}

} // namespace

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

static void prepare_start(
    SingleSwitchProgramEnvironment& env,
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, StoryEnv& story, bool at_handoff
){
    const StoryCheckpoint& checkpoint = checkpoints[first];
    if (!checkpoint.start || (checkpoint.live_start && !at_handoff)){
        require_preflight(env.console, story.context);
        env.console.log(std::format(
            "[Story] {}: starts {}, no save to load",
            checkpoint.id, checkpoint.start ? "from the game as it is" : "a new game"
        ));
        return;
    }
    load_start_save(env, story, checkpoint);
}

static void stop_for_handoff(
    SingleSwitchProgramEnvironment& env, EventNotificationOption& notification,
    const std::string& message, const std::vector<std::string>& lines = {}
){
    const std::string full = Pokemon::report_rng_handoff_stop(
        env.console, message, lines,
        [&](const std::string& line){ Pokemon::add_wrapped_overlay_log(env.console.overlay(), line, COLOR_WHITE); }
    );
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

static void run_verify_story_row(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, StoryExecutor& executor,
    const StoryCheckpoint& row, Pokemon::StoryBackend& backend,
    Pokemon::StoryGame& game, Pokemon::RunState& core_state
){
    Pokemon::AutoStoryStats& stats = env.current_stats<Pokemon::AutoStoryStats>();
    const Pokemon::VerifyRowReport report = Pokemon::run_verify_row(
        backend,
        row.id,
        *row.verify,
        [&row, &executor, &story]{
            return make_frlg_engine_checkpoint(row, executor, story.run_state);
        },
        game,
        core_state,
        [&story, &row](Pokemon::StoryBackend&, const Pokemon::StoryCheckpoint&, Pokemon::RunState&){
            return check_league_save(story, row);
        }
    );
    stats.m_reset += report.reloads;
    if (!report.passed){
        env.update_stats();
        env.console.log(std::string(STORY_LEFT_WHERE_FAILED));
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format(
                "AutoStory stopped at {}: {}",
                row.id,
                report.failure ? std::format("{}", report.failure->failure) : std::string("verify failed")
            )
        );
    }
    stats.m_checkpoint++;
}

static void run_story_rows(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, StoryExecutor& executor,
    const std::vector<StoryCheckpoint>& rows, const Pokemon::StoryRunPolicy& policy,
    std::function<void()> before
){
    FrlgConsoleBackend backend(env, story.context);
    Pokemon::StoryGame game = make_frlg_story_game(env, story, executor, rows, policy, std::move(before));
    Pokemon::RunState core_state;

    backend.log(std::format("[Segment] 00: start, {} checkpoints", rows.size()));
    size_t index = 0;
    try{
        while (index < rows.size()){
            if (rows[index].verify){
                run_verify_story_row(env, story, executor, rows[index], backend, game, core_state);
                index++;
                continue;
            }
            const size_t chunk_start = index;
            while (index < rows.size() && !rows[index].verify){
                index++;
            }
            std::vector<Pokemon::StoryCheckpoint> chunk;
            chunk.reserve(index - chunk_start);
            for (size_t c = chunk_start; c < index; c++){
                chunk.push_back(make_frlg_engine_checkpoint(rows[c], executor, story.run_state));
            }
            if (const std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(backend, chunk, game, core_state)){
                env.update_stats();
                env.console.log(std::string(STORY_LEFT_WHERE_FAILED));
                throw OperationFailedException(
                    ErrorReportMode::NO_ERROR_REPORT,
                    std::format("AutoStory stopped at {}: {}", failure->checkpoint_id, failure->failure)
                );
            }
            env.current_stats<Pokemon::AutoStoryStats>().m_checkpoint += chunk.size();
        }
    }catch (OperationFailedException&){
        env.update_stats();
        throw;
    }
    env.update_stats();
    backend.log(std::format("[Segment] 00: done, {} checkpoints", rows.size()));
    env.console.log(std::format("AutoStory: done through {}.", rows.back().id));
}

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

const StarterRngLoopLimits RNG_LOOP_LIMITS{
    .attempt_time = std::chrono::hours(3),
    .total_time = std::chrono::hours(12),
    .max_resets = 50,
    .max_retries = 2,
};

} // namespace

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
        options.SID.set(confirmed_sid);
        env.console.log(
            "AutoStory: set the RNG plan's SID to " + confirmed_sid + ". The settings file gets it "
            "when the program's panel is closed or the app quits.", COLOR_BLUE
        );
    }
    return result;
}

static bool run_handoff_to_shiny(
    SingleSwitchProgramEnvironment& env, EventNotificationOption& finished,
    const std::string& program, const Pokemon::RngHandoffCall& call
){
    Pokemon::RngHandoffResult result;
    const std::optional<std::string> user_turn = Pokemon::run_rng_handoff_or_user_turn(
        env.console, program, [&]{
            result = call();
            return result;
        }
    );
    if (user_turn){
        stop_for_handoff(env, finished, *user_turn);
        return false;
    }
    Pokemon::add_wrapped_overlay_log(env.console.overlay(), result.summary, COLOR_WHITE);
    return true;
}

static bool run_starter_rng_in_program(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngPlanned& planned, StarterRngPlanOptions& options, Language language,
    const StarterRngNotifications& notifications, EventNotificationOption& finished
){
    RngStopCalibrationOptions& calibration = options.CALIBRATION;
    ConsoleStarterRngSteps console_steps(env, context, StarterRngConsoleSettings{
        .language = language, .sound = options.SOUND, .calibration = calibration.calibration(calibration.STARTER_INGAME),
    }, notifications);
    CalibrationSavingSteps steps(console_steps, calibration, calibration.STARTER_INGAME);
    return run_handoff_to_shiny(env, finished, "Starter RNG", [&]{
        const StarterRngLoopResult result = run_rng_plan_in_program(env, steps, planned, options);
        return rng_handoff_result(result, starter_rng_loop_message(result));
    });
}

static StarterRngLoopResult run_gift_rng_in_program(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngPlanned& planned, StarterRngPlanOptions& options, GiftRngConsoleSettings settings,
    const GiftRngNotifications& notifications
){
    RngStopCalibrationOptions& calibration = options.CALIBRATION;
    settings.sound = options.SOUND;
    settings.calibration = calibration.calibration(calibration.GIFT_INGAME);
    ConsoleGiftRngSteps console_steps(env, context, settings, notifications);
    CalibrationSavingSteps steps(console_steps, calibration, calibration.GIFT_INGAME);
    return run_rng_plan_in_program(env, steps, planned, options);
}

static StoryRunPlan after_shiny_starter(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context, StoryEnv& story, StoryRunPlan plan
){
    pbf_mash_button(context, BUTTON_B, 5000ms);
    context.wait_for_all_requests();
    env.console.log("The shiny Squirtle isn't saved until " + plan.rows.front().id + " saves after the rival's battle. "
        "If it fails before then, the run stops without reloading: save by hand.", COLOR_BLUE);
    story.has_save = false;
    story.saved_checkpoint_id.clear();
    return plan;
}

static StoryRunPlan after_shiny_gift(SingleSwitchProgramEnvironment& env, StoryEnv& story, StoryRunPlan plan){
    env.console.log("The shiny gift isn't saved until " + plan.rows.front().id + " saves. "
        "If it fails before then, the run stops without reloading: save by hand.", COLOR_BLUE);
    story.has_save = false;
    story.saved_checkpoint_id.clear();
    return plan;
}

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

Pokemon::RngHandoffMode AutoStory::starter_rng_mode() const{
    return Pokemon::rng_handoff_mode({.stop = STOP_FOR_STARTER_RNG, .run = RUN_STARTER_RNG});
}

StoryHandoffs AutoStory::story_handoffs() const{
    return StoryHandoffs{
        .sid_helper = RUN_STARTER_RNG ? static_cast<std::string>(RNG_PLAN.SID).empty() : bool(STOP_FOR_SID_HELPER),
        .starter_rng = starter_rng_mode() != Pokemon::RngHandoffMode::OFF,
        .gift_rng = RUN_GIFT_RNG,
    };
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    if (const std::optional<Pokemon::VerifyPolicy> verify = RUN.verify_policy()){
        verify_story_checkpoint(env, context, RUN, *verify, NOTIFICATION_PROGRAM_FINISH);
        return;
    }
    const std::vector<StoryCheckpoint> checkpoints = load_run_rows(env.console);
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
        RNG_PLAN.check(env.console);
    }
    if (plan.stop == StoryHandoff::SID_HELPER){
        set_sid_in_program(env, context, player_name, PLAYER_GENDER, RNG_PLAN, RUN.language());
        first = checkpoint_index(checkpoints, "01.02");
        plan = plan_story_run(checkpoints, first, last, handoffs);
    }
    StoryEnv story = story_env(env, context, checkpoints[first], RUN.check_level(), KEEP_START_FRAME);
    keep_field_move_users(env.console, story, FIELD_MOVE_USERS);
    story.player_gender = PLAYER_GENDER;
    story.player_name = player_name;
    story.take_master_ball = TAKE_MASTER_BALL;
    story.nugget_farm_count = NUGGET_FARM_COUNT;
    const bool at_gift_rng = plan.stop == StoryHandoff::GIFT_RNG && plan.rows.empty();
    prepare_start(env, checkpoints, first, story, at_gift_rng);
    story.language = RUN.language();
    set_breakpoints(env, context, story, RUN.breakpoints());
    StoryExecutor executor(story);
    executor.set_pause_points(&story.pause);

    const std::optional<std::string> done_through = at_gift_rng
        ? run_from_gift_rng_save(env, context, story, executor, checkpoints, first)
        : run_story_plan(env, context, story, executor, checkpoints, std::move(plan));
    if (done_through){
        send_story_finished(env, NOTIFICATION_PROGRAM_FINISH, *done_through, sid_before, RNG_PLAN.SID);
    }
}

std::optional<std::string> AutoStory::run_story_plan(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryEnv& story, StoryExecutor& executor, const std::vector<StoryCheckpoint>& rows, StoryRunPlan plan
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
        const size_t row = checkpoint_index(rows, last_id);
        std::optional<StoryRunPlan> next;
        switch (stop){
        case StoryHandoff::NONE:
            return last_id;
        case StoryHandoff::SID_HELPER:
            throw InternalProgramError(&env.logger(), PA_CURRENT_FUNCTION, "SID Helper's stop has no rows to run.");
        case StoryHandoff::STARTER_RNG:
            next = after_starter_ball(env, context, story, planned, rows, row);
            break;
        case StoryHandoff::GIFT_RNG:
            next = after_gift_row(env, context, story, planned, rows, row);
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
    StoryEnv& story, StoryExecutor& executor, const std::vector<StoryCheckpoint>& rows, size_t first
){
    const StarterRngPlanned planned = plan_handoff_attempts(
        env, context, RNG_PLAN, RUN.language(), StoryHandoff::GIFT_RNG, rows[first].id
    );
    std::optional<StoryRunPlan> next = after_gift_row(env, context, story, planned, rows, first - 1);
    if (!next){
        return std::nullopt;
    }
    return run_story_plan(env, context, story, executor, rows, std::move(*next));
}

std::optional<StoryRunPlan> AutoStory::after_starter_ball(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryEnv& story, const StarterRngPlanned& planned, const std::vector<StoryCheckpoint>& rows, size_t ball_row
){
    if (starter_rng_mode() != Pokemon::RngHandoffMode::RUN){
        stop_for_starter_rng(env, NOTIFICATION_PROGRAM_FINISH, RNG_PLAN.sound_name(), rng_attempt_lines(planned.attempts));
        return std::nullopt;
    }
    const StarterRngNotifications notifications{
        .shiny = NOTIFICATION_SHINY, .status_update = NOTIFICATION_STATUS_UPDATE, .error_recoverable = NOTIFICATION_ERROR_RECOVERABLE,
    };
    if (!run_starter_rng_in_program(env, context, planned, RNG_PLAN, RUN.language(), notifications, NOTIFICATION_PROGRAM_FINISH)){
        return std::nullopt;
    }
    return after_shiny_starter(
        env, context, story, rows_after_starter_rng(rows, ball_row, RUN.end_index(), story_handoffs())
    );
}

std::optional<StoryRunPlan> AutoStory::after_gift_row(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryEnv& story, const StarterRngPlanned& planned, const std::vector<StoryCheckpoint>& rows, size_t gift_row
){
    const GiftRngConsoleSettings settings{
        .language = RUN.language(), .sound = {}, .calibration = {}, .target = rows[gift_row].gift,
    };
    const GiftRngNotifications notifications{
        .shiny = NOTIFICATION_SHINY, .status_update = NOTIFICATION_STATUS_UPDATE, .error_recoverable = NOTIFICATION_ERROR_RECOVERABLE,
    };
    const std::string& saving_row = rows.at(gift_row + 1).id;
    const bool shiny = run_handoff_to_shiny(env, NOTIFICATION_PROGRAM_FINISH, "Gift RNG", [&]{
        const StarterRngLoopResult result = run_gift_rng_in_program(env, context, planned, RNG_PLAN, settings, notifications);
        return rng_handoff_result(result, gift_rng_loop_message(result, settings.target, saving_row));
    });
    if (!shiny){
        return std::nullopt;
    }
    return after_shiny_gift(env, story, rows_after_gift_rng(rows, gift_row, RUN.end_index(), story_handoffs()));
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
