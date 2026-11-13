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
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_StoryPreflight.h"
#include "Pokemon/AutoStory/Pokemon_StoryRun.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Options/Pokemon_StoryRunOptions.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameEntry.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Actions.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryLanguage.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


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

//  The start and end dropdowns' entries: segments 00 and 01 joined, 00.00
//  to 01.11. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        std::vector<std::string> labels;
        for (const StoryCheckpoint& row : story_checkpoints()){
            labels.push_back(story_checkpoint_label(row));
        }
        return Pokemon::story_checkpoint_database(labels);
    }();
    return database;
}

std::set<std::string> rows_that_stop(const std::vector<StoryCheckpoint>& rows){
    std::set<std::string> ids;
    for (const StoryCheckpoint& row : rows){
        if (row.on_failure == StoryFailure::STOP){
            ids.insert(row.id);
        }
    }
    return ids;
}

//  What BDSP lends the shared shell: the console's preflight; a load is a
//  reset from HOME, which loads the save the row made at its start; the
//  engine's lines go to the console's log.
Pokemon::StoryGameHooks story_game(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    Pokemon::StoryGameHooks game;
    game.preflight = [&env, &context]{
        return Pokemon::StoryPreflight::read(env.console, context);
    };
    game.load_save = [&env, &context](Pokemon::RunState&){
        if (!reset_game_from_home(env, env.console, context, true, 1000ms)){
            return Pokemon::outcome_failed("reload_failed", "Failed to reset the game from HOME.");
        }
        return Pokemon::outcome_ok();
    };
    game.log = [&env](const std::string& line){
        env.log(line, COLOR_BLUE);
    };
    game.status = [&env](const std::string& line){
        env.console.overlay().add_log(line);
    };
    return game;
}

//  Verify mode: the checkpoint run as `policy` says, from the save the
//  start loaded. Rows save at their start, never at their end, so each
//  reload between runs loads that same save. Throws unless enough runs pass.
std::string verify_story_checkpoint(
    const std::string& id, const Pokemon::VerifyPolicy& policy,
    Pokemon::Segment& segment, Pokemon::StoryGameHooks& game, Pokemon::RunState& state
){
    Pokemon::SegmentHooks hooks = Pokemon::story_segment_hooks(game);
    Pokemon::VerifyReport report;
    try{
        report = Pokemon::verify_checkpoint(id, policy, segment, hooks, state);
    }catch (OperationFailedException&){
        Pokemon::log_story_failure(game);
        throw;
    }
    return Pokemon::finish_story_verify(id, policy, report, game);
}

} // namespace


BDSPAutoStory::BDSPAutoStory()
    : RUN(checkpoint_database(), Pokemon::Gen8ToGen9Languages_Database())
    , NEW_GAME_INTRO(
        "<b>Start a new game first:</b><br>"
        "For a game with no save: restart it and play the intro (language, appearance, names) "
        "until the player is in control in the bedroom. Only when starting at 00.00.",
        LockMode::LOCK_WHILE_RUNNING, false
    )
    , PLAYER_NAME(false, "<b>Player name:</b><br>For a new game. Letters and digits.", LockMode::LOCK_WHILE_RUNNING, "Kosmos", "Kosmos")
    , PLAYER_LOOK(
        "<b>Player's look:</b><br>For a new game: the look picked on the appearance grid. "
        "The portraits row by row: 1 Lucas light, 2 Lucas dark, 3 Dawn dark-haired, 4 Dawn dark; "
        "5 Lucas blond, 6 Lucas brown, 7 Dawn blond, 8 Dawn brown-haired."
    )
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
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(NEW_GAME_INTRO);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(PLAYER_LOOK);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(NOTIFICATIONS);
}

namespace{

//  The checkpoints whose start save is skipped: the first one, unless the
//  run starts a new game. After a load its save is already there (saving
//  again cost ~10 s), and nothing saves before the start is confirmed.
std::set<std::string> saved_before_the_run(const std::vector<StoryCheckpoint>& checkpoints, size_t first, Pokemon::StoryStart start){
    if (Pokemon::story_first_save_allowed(start)){
        return {};
    }
    return {checkpoints[first].id};
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

//  Gets the game to where `row` starts through the shared start gate:
//  preflight, then (for a save) the load and the save card's place. Throws a
//  setup error when the preflight failed, nothing pressed, and an
//  OperationFailedException when the load or place check failed, unsaved.
void start_story(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context, const StoryCheckpoint& row,
    Pokemon::StoryStart start, Pokemon::StoryGameHooks& game, Pokemon::RunState& state
){
    //  The loaded save must be where the start row saved; a retry's reload
    //  in a later row is checked by that row's own start save.
    game.check_save_place = [&env, &context, place = row.saves_in]{
        return check_start_place(env, context, place);
    };
    const Pokemon::Outcome loaded = Pokemon::prepare_story_start(row.id, start, game, state);
    game.check_save_place = nullptr;
    if (loaded.failure == Pokemon::StoryPreflight::FAILURE){
        throw UserSetupError(env.console, std::format("AutoStory did not start, nothing was pressed: {}", loaded.detail));
    }
    if (!loaded.succeeded()){
        Pokemon::log_story_failure(game);
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("AutoStory could not load the save for {}: {}", row.id, Pokemon::to_string(loaded))
        );
    }
}

//  The run from `range`'s first checkpoint to its last, its counts in the
//  stats. Returns the finish line; throws if it stopped short.
std::string run_story(
    const Pokemon::SegmentRange& range, const Pokemon::SegmentPolicy& policy,
    Pokemon::Segment& segment, Pokemon::StoryGameHooks& game, Pokemon::RunState& state,
    SingleSwitchProgramEnvironment& env
){
    Pokemon::SegmentHooks hooks = Pokemon::story_segment_hooks(game);
    Pokemon::SegmentReport report;
    try{
        report = run_segment(range, policy, segment, hooks, state);
    }catch (OperationFailedException&){
        Pokemon::log_story_failure(game);
        throw;
    }
    Pokemon::AutoStoryStats& stats = env.current_stats<Pokemon::AutoStoryStats>();
    std::string done;
    try{
        done = Pokemon::finish_story_run(report, range.to.value_or(""), stats, game);
    }catch (OperationFailedException&){
        env.update_stats();
        throw;
    }
    env.update_stats();
    return done;
}

} // namespace

void BDSPAutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= checkpoints.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    //  The reads that use text (Location.h) take the game's language from here.
    set_story_language(RUN.language());

    StoryEnv story{env, context, STARTER, PLAYER_NAME, IntroPicks{RUN.language(), PLAYER_LOOK.index()}, RUN.check_level()};
    Pokemon::StoryPausePoints pause(RUN.breakpoints(), Pokemon::make_story_pause_hook(context));
    pause.set_log([&env](std::string_view line){
        env.console.log(std::string(line));
    });
    StoryExecutor executor(story);
    executor.set_breakpoints(&pause);
    executor.set_stop_rows(rows_that_stop(checkpoints));
    //  A start with a save always loads it first, so the run never walks
    //  from wherever the game was left (console B, 2026-11-11: a run from
    //  00.04b walked from 00.04d's position).
    const bool new_game = first == 0 && NEW_GAME_INTRO;
    const Pokemon::StoryStart start = new_game ? Pokemon::StoryStart::NEW_GAME : Pokemon::StoryStart::FROM_SAVE;
    if (new_game && RUN.VERIFY.policy()){
        throw UserSetupError(env.console, "Verify mode reloads the start's save between runs: turn off \"Start a new game first\".");
    }
    std::set<std::string> saved = saved_before_the_run(checkpoints, first, start);
    Pokemon::Segment segment = make_story_segment(checkpoints, executor, saved, RUN.policy().save_mode);
    Pokemon::StoryGameHooks game = story_game(env, context);
    Pokemon::RunState state;
    start_story(env, context, checkpoints[first], start, game, state);
    if (const std::optional<Pokemon::VerifyPolicy> verify = RUN.VERIFY.policy()){
        const std::string summary = verify_story_checkpoint(checkpoints[first].id, *verify, segment, game, state);
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, summary);
        return;
    }
    const std::string done = run_story(
        Pokemon::SegmentRange{checkpoints[first].id, checkpoints[last].id}, RUN.policy(), segment, game, state, env
    );
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, done);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
