/*  BDSP AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include <format>
#include <optional>
#include <set>
#include <string_view>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/StaticGlobals.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Pokemon_StoryPreflight.h"
#include "Pokemon/AutoStory/Pokemon_StoryRngHandoff.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "PokemonBDSP/Programs/RngManipulation/PokemonBDSP_StarterRngAttempts.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Actions.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryLanguage.h"
#include "PokemonBDSP_AutoStory_ConsoleStory.h"
#include "PokemonBDSP_AutoStory_Launch.h"
#include "PokemonBDSP_AutoStory_StoryData.h"
#include "PokemonBDSP_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


BDSPAutoStory_Descriptor::BDSPAutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonBDSP:AutoStory",
        "Pokemon BDSP", "Auto Story",
        "Programs/PokemonBDSP/AutoStory.html",
        "Automatically completes BDSP story segments.",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        {"Tesseract"}
    )
{}

std::unique_ptr<StatsTracker> BDSPAutoStory_Descriptor::make_stats() const{
    return std::make_unique<Pokemon::AutoStoryStats>();
}


namespace{

//  What the user does around Starter RNG when the run stops for it: the
//  option's help and the run's closing message.
constexpr std::string_view STARTER_RNG_HANDOFF_TEXT =
    "At the end of 00.01 save at the Verity Lakefront (17, 14), facing Lake Verity's entrance, and stop. "
    "Run the Starter RNG program from that save (Start point: Verity Lakefront). "
    "When it is done, close the Summary page and the menu, let Rowan's scene finish so the player stands "
    "on the Verity Lakefront overworld, save, and start AutoStory at 00.03.";

std::string stop_for_starter_rng_help(){
    return std::format(
        "<b>Stop for Starter RNG:</b><br>Off by default. Ignored while Run Starter RNG in AutoStory is on. {}",
        STARTER_RNG_HANDOFF_TEXT
    );
}

constexpr std::string_view PLAYER_NAME_HELP =
    "<b>Player name:</b><br>For a new game. Letters and digits, typed on the Switch's Latin keyboard. "
    "If the game takes fewer characters in its language, the intro stops at the name screen.";

constexpr std::string_view PLAYER_LOOK_HELP =
    "<b>Player's look:</b><br>For a new game: the look picked on the appearance grid. "
    "The portraits row by row: 1 Lucas light, 2 Lucas dark, 3 Dawn dark-haired, 4 Dawn dark; "
    "5 Lucas blond, 6 Lucas brown, 7 Dawn blond, 8 Dawn brown-haired. "
    "The new game's language is Game language above (Japanese: the kana one).";

constexpr std::string_view CAPTURE_INTRO_SCREENS_HELP =
    "<b>Capture intro screens (developer):</b><br>Instead of a run: for a game with no save, running in front. "
    "For each language the game lists, restarts the game, picks the language and the look above, "
    "and records what shows after the look, pressing nothing, to the Screenshots folder as "
    "&lt;lang&gt;_after-look_NN.png (a frame per change and per second, until the screen holds still). "
    "Also saves &lt;lang&gt;_before-look-pick.png, and en_before-language-pick.png on the first round. "
    "Never types a name; ends where the last recording ended.";

//  The start and end dropdowns' entries: segments 00 and 01 joined, 00.00
//  to 01.11. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        StringSelectDatabase db;
        const std::vector<StoryCheckpoint> rows = bdsp_rows_for_mode(load_bdsp_story({}), STATIC_GLOBALS.DEVELOPER_MODE);
        for (size_t i = 0; i < rows.size(); i++){
            db.add_entry(StringSelectEntry(std::to_string(i), Pokemon::story_checkpoint_label(rows[i].id, rows[i].title)));
        }
        return db;
    }();
    return database;
}

//  The rows whose failure ends the program: the rows that save nothing and
//  retry nothing, and the plan's own (Starter RNG's, which resets by itself).
std::set<std::string> rows_that_stop(const StoryRunPlan& plan){
    std::set<std::string> ids = plan.fatal_rows;
    for (const StoryCheckpoint& row : plan.rows){
        if (row.on_failure == StoryFailurePolicy::STOP){
            ids.insert(row.id);
        }
    }
    return ids;
}

//  "" when the save screen's card names `place`, the map the start row
//  saved in, else why not. Opens the save screen and backs out unsaved.
std::string check_start_place(SingleSwitchProgramEnvironment& env, ProControllerContext& context, const std::string& place){
    if (place.empty()){
        return "";
    }
    const std::string read = read_location_from_save_screen(env.console, context);
    if (location_banner_matches(read, place)){
        return "";
    }
    return std::format("The save is in \"{}\", not {}.", read, place);
}

//  Runs preflight, then (for a save start) loads the save and checks its card.
//  Throws UserSetupError on preflight failure (nothing pressed), or
//  OperationFailedException if the load or card check failed.
void start_story(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StoryCheckpoint& start_row, bool new_game, ConsoleStory& executor
){
    if (const std::optional<Pokemon::StoryFailure> preflight = Pokemon::StoryPreflight::read(env.console, context).check()){
        throw UserSetupError(env.console, std::format("AutoStory did not start, nothing was pressed: {}", preflight->detail));
    }
    if (!new_game){
        if (const std::optional<Pokemon::StoryFailure> loaded = launch_bdsp_save(env.console, context)){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                std::format("AutoStory could not load the save for {}: {}", start_row.id, *loaded)
            );
        }
        const std::string wrong_place = check_start_place(env, context, start_row.saves_in);
        if (!wrong_place.empty()){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                std::format("AutoStory could not start {}: {}", start_row.id, wrong_place)
            );
        }
    }
    executor.run_state() = BdspRunState{.pose = {}, .save_loaded = !new_game};
}

//  Verify mode: runs the start checkpoint up to `policy.runs` times from its
//  loaded save without saving at the end. Throws unless enough runs pass.
std::string verify_story_checkpoint(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StoryCheckpoint& row, const Pokemon::VerifyPolicy& policy, ConsoleStory& executor
){
    size_t passes = 0;
    size_t runs_done = 0;
    for (size_t run = 0; run < policy.runs; run++){
        if (run > 0){
            if (const std::optional<Pokemon::StoryFailure> loaded = launch_bdsp_save(env.console, context)){
                throw OperationFailedException(
                    ErrorReportMode::NO_ERROR_REPORT,
                    std::format("Verify reload failed for {}: {}", row.id, *loaded)
                );
            }
            executor.run_state() = BdspRunState{.pose = {}, .save_loaded = true};
        }
        std::vector<Pokemon::StoryCheckpoint> single = executor.make_checkpoints(std::span<const StoryCheckpoint>(&row, 1));
        Pokemon::StoryGame game{.save = {}, .reload = {}, .attempts_per_checkpoint = 1};
        Pokemon::RunState engine_state;
        const std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(
            executor.backend(), single, game, engine_state
        );
        runs_done++;
        if (!failure){
            passes++;
        }
        const size_t remaining = policy.runs - runs_done;
        if (passes >= policy.passes_needed || passes + remaining < policy.passes_needed){
            break;
        }
    }
    const std::string summary = std::format(
        "Verify {}: {}/{} passed (needed {}).", row.id, passes, runs_done, policy.passes_needed
    );
    env.log(summary, passes >= policy.passes_needed ? COLOR_BLUE : COLOR_RED);
    env.console.overlay().add_log(summary);
    if (passes < policy.passes_needed){
        throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, summary);
    }
    return summary;
}

Pokemon::StoryGame make_story_game(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StoryRunPlan& plan, const Pokemon::StoryRunPolicy& policy,
    ConsoleStory& executor, Pokemon::AutoStoryStats& stats,
    std::optional<std::string>& saved_start
){
    Pokemon::StoryGame game;
    game.attempts_per_checkpoint = policy.attempts_per_checkpoint;
    game.save = [&](Pokemon::StoryBackend&, const Pokemon::StoryCheckpoint& passed) -> std::optional<Pokemon::StoryFailure>{
        stats.m_checkpoint++;
        env.update_stats();
        const auto it = std::find_if(plan.rows.begin(), plan.rows.end(), [&](const StoryCheckpoint& row){
            return row.id == passed.id;
        });
        if (it != plan.rows.end() && it + 1 != plan.rows.end()){
            const StoryCheckpoint& next = *(it + 1);
            if (next.on_failure == StoryFailurePolicy::RELOAD_AND_RETRY){
                if (const std::optional<Pokemon::StoryFailure> err = executor.save_checkpoint(next.saves_in)){
                    return err;
                }
                saved_start = next.id;
            }
            return std::nullopt;
        }
        if (plan.stop == StoryHandoff::STARTER_RNG && passed.id == STARTER_RNG_HANDOFF_ROW && !plan.handoff_saves_in.empty()){
            return executor.save_checkpoint(plan.handoff_saves_in);
        }
        return std::nullopt;
    };
    game.reload = [&](Pokemon::StoryBackend&, const Pokemon::StoryCheckpoint&) -> std::optional<std::string>{
        if (!saved_start){
            return std::nullopt;
        }
        stats.m_reset++;
        env.update_stats();
        if (const std::optional<Pokemon::StoryFailure> loaded = launch_bdsp_save(env.console, context)){
            env.log(std::format("Reload failed: {}", *loaded), COLOR_RED);
            return std::nullopt;
        }
        executor.run_state() = BdspRunState{.pose = {}, .save_loaded = true};
        return saved_start;
    };
    return game;
}

//  Runs `plan` through #648 Pokemon::run_checkpoints() with E9 save-at-end:
//  checkpoint N saves at its end for checkpoint N+1 (checking N+1's saves_in
//  card), or for Starter RNG handoff at the end of 00.01.
std::string run_story(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StoryRunPlan& plan, const Pokemon::StoryRunPolicy& policy, bool new_game, ConsoleStory& executor
){
    std::vector<Pokemon::StoryCheckpoint> checkpoints = executor.make_checkpoints(plan.rows);
    std::optional<std::string> saved_start = new_game ? std::nullopt : std::optional<std::string>(plan.rows.front().id);
    Pokemon::AutoStoryStats& stats = env.current_stats<Pokemon::AutoStoryStats>();
    stats.m_segment++;
    env.update_stats();

    Pokemon::StoryGame game = make_story_game(env, context, plan, policy, executor, stats, saved_start);
    Pokemon::RunState engine_state;
    const std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(
        executor.backend(), checkpoints, game, engine_state
    );
    env.update_stats();
    if (failure){
        const std::string detail = std::format(
            "AutoStory stopped at {}: {}", failure->checkpoint_id, failure->failure
        );
        env.log(detail, COLOR_RED);
        env.console.overlay().add_log(detail);
        throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, detail);
    }
    const std::string done = std::format(
        "AutoStory completed {} to {}.", plan.rows.front().id, plan.rows.back().id
    );
    env.log(done, COLOR_BLUE);
    env.console.overlay().add_log(done);
    return done;
}

//  The rows for this run: the built-in checkpoints, with those of
//  <user files>/AutoStoryDev/bdsp/ in their place, read now so a run after
//  an edit plays the edited walks. Logs each dev file it uses; a bad one
//  fails the run.
std::vector<StoryCheckpoint> load_run_rows(ConsoleHandle& console){
    const std::string folder = Pokemon::story_dev_folder("bdsp");
    try{
        const BdspStoryData story = load_bdsp_story(Pokemon::read_story_dev_files(folder, "bdsp"));
        for (const auto& [name, path] : story.dev_files){
            console.log(std::format("AutoStory: using dev checkpoint {} from {}", name, path));
        }
        return bdsp_rows_for_mode(story, STATIC_GLOBALS.DEVELOPER_MODE);
    }catch (const ParseException& error){
        throw UserSetupError(console, std::format("AutoStory: bad dev checkpoint in {}: {}", folder, error.message()));
    }
}

} // namespace


BDSPAutoStory::~BDSPAutoStory(){
    STARTER_RNG.run().remove_listener(*this);
}

BDSPAutoStory::BDSPAutoStory()
    : RUN(checkpoint_database(), Pokemon::Gen8ToGen9Languages_Database())
    , NEW_GAME_INTRO(
        "<b>Start a new game first:</b><br>"
        "For a game with no save: restart it and play the intro (language, appearance, names) "
        "until the player is in control in the bedroom. Only when starting at 00.00.",
        LockMode::LOCK_WHILE_RUNNING, false
    )
    , PLAYER_NAME(false, std::string(PLAYER_NAME_HELP), LockMode::LOCK_WHILE_RUNNING, "Kosmos", "Kosmos")
    , PLAYER_LOOK(std::string(PLAYER_LOOK_HELP))
    , CAPTURE_INTRO_SCREENS(std::string(CAPTURE_INTRO_SCREENS_HELP), LockMode::LOCK_WHILE_RUNNING, false)
    , STARTER(
        "<b>Starter Pokemon:</b>",
        {
            {StarterChoice::TURTWIG,  "turtwig",  "Turtwig"},
            {StarterChoice::CHIMCHAR, "chimchar", "Chimchar"},
            {StarterChoice::PIPLUP,   "piplup",   "Piplup"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StarterChoice::CHIMCHAR
    )
    , STOP_FOR_STARTER_RNG(stop_for_starter_rng_help(), LockMode::LOCK_WHILE_RUNNING, false)
    , NOTIFICATIONS({
        &STARTER_RNG.notification_shiny(),
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(NEW_GAME_INTRO);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(PLAYER_LOOK);
    PA_ADD_OPTION(CAPTURE_INTRO_SCREENS);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(STOP_FOR_STARTER_RNG);
    add_option(STARTER_RNG.run(), "RUN_STARTER_RNG");
    add_option(STARTER_RNG.player_model(), "STARTER_RNG_PLAYER_MODEL");
    add_option(STARTER_RNG.max_resets(), "MAX_RESETS");
    add_option(STARTER_RNG.filters(), "STARTER_RNG_FILTERS");
    PA_ADD_OPTION(NOTIFICATIONS);

    BDSPAutoStory::on_config_value_changed(this);
    STARTER_RNG.run().add_listener(*this);
}

//  Run Starter RNG takes precedence over Stop for Starter RNG: the stop
//  option is greyed out while it is on, and the plan ignores it.
void BDSPAutoStory::on_config_value_changed(void*){
    const bool run_in_story = STARTER_RNG.run();
    STARTER_RNG.set_details_visible(run_in_story);
    STOP_FOR_STARTER_RNG.set_visibility(run_in_story ? ConfigOptionState::DISABLED : ConfigOptionState::ENABLED);
}

void BDSPAutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    if (CAPTURE_INTRO_SCREENS){
        capture_intro_screens(env, context, PLAYER_LOOK.index());
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, "Intro screens captured.");
        return;
    }
    const std::vector<StoryCheckpoint> checkpoints = load_run_rows(env.console);
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= checkpoints.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    //  The reads that use text (Location.h) take the game's language from here.
    set_story_language(RUN.language());

    const bool new_game = first == 0 && NEW_GAME_INTRO;
    //  Starter RNG watches the player's eyes through the look's template.
    if (new_game && STARTER_RNG.run() && STARTER_RNG.player_model().index() != PLAYER_LOOK.index()){
        throw UserSetupError(env.console, "The Starter RNG character model must be the new game's look (Player's look).");
    }
    StoryEnv story{env, context, STARTER, PLAYER_NAME, IntroPicks{RUN.language(), PLAYER_LOOK.index()}, {}};
    if (STARTER_RNG.run()){
        story.run_starter_rng = make_starter_rng_hook(
            env, context, STARTER_RNG, STARTER_RNG.config(RUN.language(), STARTER), NOTIFICATION_ERROR_RECOVERABLE
        );
    }
    ConsoleStory executor(story);
    const StoryRunPlan plan = plan_story_run(
        checkpoints, first, last,
        StoryHandoffs{.starter_rng = Pokemon::rng_handoff_mode({.stop = STOP_FOR_STARTER_RNG, .run = STARTER_RNG.run()})}
    );
    executor.set_stop_rows(rows_that_stop(plan));
    run_plan(env, context, plan, new_game, executor);
}

void BDSPAutoStory::run_plan(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StoryRunPlan& plan, bool new_game, ConsoleStory& executor
){
    //  A start with a save always loads it first, so the run never walks
    //  from wherever the game was left (console B, 2026-11-11: a run from
    //  00.04b walked from 00.04d's position).
    if (new_game && RUN.VERIFY.policy()){
        throw UserSetupError(env.console, "Verify mode reloads the start's save between runs: turn off \"Start a new game first\".");
    }
    if (plan.stop != StoryHandoff::NONE && RUN.VERIFY.policy()){
        throw UserSetupError(env.console, "Verify mode repeats the start's checkpoint, and its extra save at the end of 00.01 would overwrite the Verity Lakefront save: turn off \"Stop for Starter RNG\".");
    }
    start_story(env, context, plan.rows.front(), new_game, executor);
    if (const std::optional<Pokemon::VerifyPolicy> verify = RUN.VERIFY.policy()){
        const std::string summary = verify_story_checkpoint(env, context, plan.rows.front(), *verify, executor);
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, summary);
        return;
    }
    const std::string done = run_story(env, context, plan, RUN.policy(), new_game, executor);
    if (plan.stop == StoryHandoff::STARTER_RNG){
        const std::string handoff = Pokemon::report_rng_handoff_stop(
            env.console, std::format("Stopped for Starter RNG. {}", STARTER_RNG_HANDOFF_TEXT), {},
            [&](const std::string& line){ env.console.overlay().add_log(line); }
        );
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, handoff);
        return;
    }
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, done);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
