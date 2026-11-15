/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonLZA/Inference/Map/PokemonLZA_LocationNameReader.h"
#include "Actions/PokemonLZA_AutoStoryConfirmChoices.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;


AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        std::format("{} LZA", STRING_POKEMON), "Auto Story",
        "Programs/PokemonLZA/AutoStory.html",
        "Play the main story of Pokemon Legends: Z-A. Prototype: Main Missions 01-03 as recorded on a console; plays from a new game to Mable's Rock Smash TM. "
        "Start a new game and pick its language, look and name by hand: checkpoint 01.00 starts on the Confirm Choices screen that follows. "
        "It checks that screen, presses nothing anywhere else, and picks Ready!. Later checkpoints load their save first. "
        "Set Game Language to the language the game is set to: the program reads place names in it.",
        PanelQualityStanding::DEVELOPER,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

std::unique_ptr<StatsTracker> AutoStory_Descriptor::make_stats() const{
    return std::make_unique<AutoStoryStats>();
}


namespace{

//  The start and end dropdowns' entries. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        std::vector<std::string> labels;
        for (const StoryCheckpoint& row : story_checkpoints()){
            labels.push_back(story_checkpoint_label(row));
        }
        return story_checkpoint_database(labels);
    }();
    return database;
}

constexpr std::string_view RELOAD_AT_BOUNDARIES_LABEL =
    "<b>Reload at Checkpoint Boundaries:</b><br>Reload the save before a checkpoint whose plan asks for it "
    "when the run comes from the checkpoint before. Off: run straight on. A failed checkpoint's retry reloads either way.";
constexpr std::string_view START_FROM_BACKUP_SAVE_LABEL =
    "<b>Start from the Backup Save:</b><br>The run starts by loading the backup save "
    "(title screen Up + B + X), the save before the latest. Off: it loads the latest save and, when a checkpoint "
    "with a start check is not at its start there, the backup save, then saves there (Report) before it starts. "
    "Later reloads load the latest save either way.<br>"
    "<font color=\"red\">Warning: a run from the backup save saves over the latest save (the start's Report, "
    "the game's autosaves, the checkpoints' saves). The progress in the latest save is lost.</font>";
constexpr std::string_view CAPTURE_LOOKS_LABEL =
    "<b>Save Screen Captures:</b><br>Save a screenshot each time what the program sees on screen changes, "
    "and every 2 s, to Screenshots/PLZA-capture/. For finding screens the program misses: a long run writes "
    "thousands of images.";

} // namespace


AutoStory::AutoStory()
    : LANGUAGE(
        "<b>Game Language:</b><br>The language the game is set to. The program reads text in it "
        "(the place names in the fast travel menu) and does not check that the game matches.",
        LocationNameOCR::instance().languages(),
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , RUN(checkpoint_database(), Gen8ToGen9Languages_Database())
    , STARTER(
        "<b>Starter Pokemon:</b>",
        {
            {StarterChoice::CHIKORITA, "chikorita", "Chikorita"},
            {StarterChoice::TEPIG,     "tepig",     "Tepig"},
            {StarterChoice::TOTODILE,  "totodile",  "Totodile"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StarterChoice::CHIKORITA
    )
    , RELOAD_AT_BOUNDARIES(std::string(RELOAD_AT_BOUNDARIES_LABEL), LockMode::LOCK_WHILE_RUNNING, false)
    , START_FROM_BACKUP_SAVE(std::string(START_FROM_BACKUP_SAVE_LABEL), LockMode::LOCK_WHILE_RUNNING, false)
    , CAPTURE_LOOKS(std::string(CAPTURE_LOOKS_LABEL), LockMode::LOCK_WHILE_RUNNING, false)
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(LANGUAGE);
    RUN.LANGUAGE.set_visibility(ConfigOptionState::HIDDEN);
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(RELOAD_AT_BOUNDARIES);
    PA_ADD_OPTION(START_FROM_BACKUP_SAVE);
    PA_ADD_OPTION(CAPTURE_LOOKS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

namespace{

//  Where the run takes its start from: the running game when RELOAD_AT_START
//  is off, else the backup or the latest save. Both off-reload and the
//  backup save at once is refused: keeping the game loads no save.
StartFrom start_from(ConsoleHandle& console, const Pokemon::StoryRunOptions& run, const BooleanCheckBoxOption& start_from_backup_save){
    if (run.RELOAD_AT_START){
        return start_from_backup_save ? StartFrom::BACKUP_SAVE : StartFrom::LATEST_SAVE;
    }
    if (start_from_backup_save){
        throw UserSetupError(console, "Start from the Backup Save loads a save: turn on Reload the Save at the Start.");
    }
    console.log("AutoStory: no reload at the start, keeping the running game.");
    return StartFrom::RUNNING_GAME;
}

//  Checkpoints after the new game start from a save (01.01 from the
//  platform autosave), so the run loads it first instead of assuming the
//  game is already there: the latest save, or the backup save when the
//  latest is past the start (load_checkpoint_start()); with `from`
//  RUNNING_GAME, the game as it runs. 01.00 starts without one.
void load_start_save(
    ConsoleHandle& console, StoryEnv& story, StoryBackend<ZonePose>& backend,
    const std::vector<StoryCheckpoint>& rows, size_t first, StartFrom from
){
    if (story_row_starts_without_save(rows[first])){
        return;
    }
    const Outcome loaded = load_checkpoint_start(story, backend, rows[first].id, from);
    if (!loaded.succeeded()){
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("AutoStory: could not load the save for {}: {}", rows[first].id, to_string(loaded))
        );
    }
    console.log(std::format("AutoStory: loaded the save for {}.", rows[first].id));
}

//  Connects the controller for a run from `first_row`. 01.00 starts on a
//  screen that proves the save is fresh, so it is checked before any press,
//  the connect press included.
void connect_controller(ConsoleHandle& console, ProControllerContext& context, const StoryCheckpoint& first_row){
    if (story_row_starts_without_save(first_row)){
        require_confirm_choices(console, context);
    }
    require_player(console, context, BUTTON_L);
}

//  Throws UserSetupError unless rows `first` to `last` are in a table of
//  `rows` rows, in order.
void check_checkpoint_range(ConsoleHandle& console, size_t first, size_t last, size_t rows){
    if (last >= rows){
        throw UserSetupError(console, "The checkpoint selection is out of range: pick the start and end checkpoints again.");
    }
    if (first > last){
        throw UserSetupError(console, "The start checkpoint must not come after the end checkpoint.");
    }
}

//  `run`'s attempts per checkpoint, and the boundary reload `reload_at_boundaries`
//  picks; logs which.
StoryRunPolicy run_policy(const Pokemon::StoryRunOptions& run, const BooleanCheckBoxOption& reload_at_boundaries, Logger& logger){
    StoryRunPolicy::BoundaryReload boundary_reload = StoryRunPolicy::BoundaryReload::STRAIGHT_THROUGH;
    if (reload_at_boundaries){
        logger.log("AutoStory: reloads the save at checkpoint boundaries whose plan asks for it.");
        boundary_reload = StoryRunPolicy::BoundaryReload::AT_PLAN_BOUNDARIES;
    }else{
        logger.log("AutoStory: runs straight through checkpoint boundaries, no reload.");
    }
    return {.attempts_per_checkpoint = run.ATTEMPTS_PER_CHECKPOINT, .boundary_reload = boundary_reload};
}

} // namespace

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const StoryData& story_data = builtin_story_data();
    const std::vector<StoryCheckpoint>& rows = story_data.checkpoints;
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    check_checkpoint_range(env.console, first, last, rows.size());
    const StartFrom from = start_from(env.console, RUN, START_FROM_BACKUP_SAVE);
    connect_controller(env.console, context, rows[first]);

    StoryEnv story{
        .env = env, .context = context, .language = LANGUAGE, .battle_ai = BATTLE_AI, .checks = RUN.check_level(),
        .capture_looks = CAPTURE_LOOKS,
    };
    PlzaConsoleBackend backend(story);
    const StoryGame<ZonePose> game = make_plza_story_game(story, STARTER, story_data.autosaves);
    StoryEngine<ZonePose> engine(game, backend);
    RunState state;
    load_start_save(env.console, story, backend, rows, first, from);

    const std::vector<StoryCheckpoint> checkpoints = story_run_rows(std::span<const StoryCheckpoint>(rows), first, last);
    const SegmentReport report = run_plza_segment(game, story_data, checkpoints, run_policy(RUN, RELOAD_AT_BOUNDARIES, env.console), engine, backend, state);
    record_story_counts(report.story_counts(), env.current_stats<AutoStoryStats>());
    env.update_stats();
    if (!report.completed){
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format(
                "AutoStory stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, to_string(report.failure)
            )
        );
    }
    env.console.log(std::format("AutoStory: done through {}.", rows[last].id));
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
