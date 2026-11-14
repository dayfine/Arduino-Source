/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/StaticGlobals.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Actions/PokemonLZA_AutoStoryIntro.h"
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
        "Play the main story of Pokemon Legends: Z-A. Prototype: Main Missions 01-03 as recorded on a console; plays from a new game to Mable's Rock Smash TM. Checkpoint 01.00 (a new game) starts on the new game's Confirm Choices screen, with the language, look and name chosen by hand: it checks that screen, presses nothing anywhere else, and picks Ready!. Checkpoint 00.00 makes those choices too: it starts on the Switch HOME screen with PLZA's tile selected and no save, launches the selected tile without moving the cursor (closing it first if it is running) and plays the new game to the platform; a run from 00.00 skips 01.00. Go Home When Done leaves HOME with PLZA's tile selected, ready for the next new game.",
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

//  The built-in rows a run may play (story_rows_for_mode()): the dev-only
//  rows only in Developer Mode. The dropdowns' indices name these rows.
std::vector<StoryCheckpoint> panel_rows(const StoryData& story){
    return story_rows_for_mode(story, STATIC_GLOBALS.DEVELOPER_MODE);
}

//  The start and end dropdowns' entries. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        std::vector<std::string> labels;
        for (const StoryCheckpoint& row : panel_rows(builtin_story_data())){
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
    "with a start check is not at its start there, the backup save. Later reloads load the latest save either way.";

} // namespace


AutoStory::AutoStory()
    : RUN(checkpoint_database(), Gen8ToGen9Languages_Database())
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
    , PLAYER_NAME(
        false,
        "<b>Player Name:</b><br>New game only. Letters and digits; a capital only as the first letter.",
        LockMode::LOCK_WHILE_RUNNING,
        "Kosmos",
        "Kosmos"
    )
    , PLAYER_LOOK(
        "<b>Player Look:</b><br>New game only. 1 to 8: left to right along the top row, then the bottom row.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, (uint8_t)LOOK_COUNT
    )
    , NAME_FIRST_LETTER_AUTO_CAPS(
        "<b>Keyboard Capitalizes the First Letter:</b><br>New game only. Tick if the name comes out in lower case: the name keyboard capitalizes by itself and Shift would undo it.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(RELOAD_AT_BOUNDARIES);
    PA_ADD_OPTION(START_FROM_BACKUP_SAVE);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(PLAYER_LOOK);
    PA_ADD_OPTION(NAME_FIRST_LETTER_AUTO_CAPS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

namespace{

//  Checkpoints after the new game start from a save (01.01 from the
//  platform autosave), so the run loads it first instead of assuming the
//  game is already there: the latest save, or the backup save when the
//  latest is past the start (load_checkpoint_start()). 00.00 and 01.00
//  start without one.
void load_start_save(
    ConsoleHandle& console, StoryEnv& story, StoryBackend<ZonePose>& backend,
    const std::vector<StoryCheckpoint>& rows, size_t first, bool backup_first
){
    if (story_row_starts_without_save(rows[first])){
        return;
    }
    const Outcome loaded = load_checkpoint_start(story, backend, rows[first].id, backup_first);
    if (!loaded.succeeded()){
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("AutoStory: could not load the save for {}: {}", rows[first].id, to_string(loaded))
        );
    }
    console.log(std::format("AutoStory: loaded the save for {}.", rows[first].id));
}

//  The story for this run: the built-in checkpoints, with those of
//  story_dev_folder() in their place, read now so a run after an edit
//  plays the edited walks. Logs each dev file it uses; a bad one fails the
//  run.
StoryData load_run_story(ConsoleHandle& console){
    const std::string folder = story_dev_folder();
    try{
        StoryData data = load_story_data(read_story_dev_files(folder));
        for (const auto& [name, path] : data.dev_files){
            console.log(std::format("AutoStory: using dev checkpoint {} from {}", name, path));
        }
        return data;
    }catch (ParseException& e){
        throw UserSetupError(console, std::format("AutoStory: bad dev checkpoint in {}: {}", folder, e.message()));
    }
}

//  Connects the controller for a run from `first_row`. 01.00 starts on a
//  screen that proves the save is fresh, so it is checked before any press,
//  the connect press included.
void connect_controller(ConsoleHandle& console, ProControllerContext& context, const StoryCheckpoint& first_row){
    if (story_row_confirms_new_game(first_row)){
        require_confirm_choices(console, context);
    }
    require_player(console, context, BUTTON_L);
}

//  PLZA's game for a run on `story` with `starter`, pausing at `breakpoints` (the
//  BREAKPOINTS option): before each step, or each leg too, where a lab build
//  waits for the user (Pokemon_StoryBreakpoints.h).
StoryGame<ZonePose> run_game(
    StoryEnv& story, StarterChoice starter, const StoryData& data, StoryBreakpoints breakpoints, CancellableScope& scope
){
    StoryGame<ZonePose> game = make_plza_story_game(story, starter, data.autosaves);
    game.breakpoints = breakpoints;
    game.pause_point = make_story_pause_hook(scope);
    return game;
}

} // namespace

NewGameChoices AutoStory::new_game_choices(ConsoleHandle& console, bool check) const{
    NewGameChoices choices{.name = PLAYER_NAME, .look = PLAYER_LOOK, .name_first_letter_auto_caps = NAME_FIRST_LETTER_AUTO_CAPS};
    const std::string problem = check ? new_game_problem(RUN.language(), choices) : std::string();
    if (!problem.empty()){
        throw UserSetupError(console, problem);
    }
    return choices;
}

void AutoStory::start_program_border_check(VideoStream& stream, FeedbackType feedback_type){
    const std::vector<StoryCheckpoint> rows = panel_rows(builtin_story_data());
    const size_t first = RUN.start_index();
    if (first < rows.size() && story_row_launches_game(rows[first])){
        stream.log(std::format("AutoStory: {} launches the game; the black border check waits for HOME.", rows[first].id));
        return;
    }
    SingleSwitchProgramInstance::start_program_border_check(stream, feedback_type);
}

namespace{

//  `run`'s attempts per checkpoint, and the boundary reload `reload_at_boundaries`
//  picks; logs which.
StoryRunPolicy run_policy(const Pokemon::StoryRunOptions& run, const BooleanCheckBoxOption& reload_at_boundaries, Logger& logger){
    if (reload_at_boundaries){
        logger.log("AutoStory: reloads the save at checkpoint boundaries whose plan asks for it.");
        return {.attempts_per_checkpoint = run.policy().attempts_per_checkpoint, .boundary_reload = StoryRunPolicy::BoundaryReload::AT_PLAN_BOUNDARIES};
    }
    logger.log("AutoStory: runs straight through checkpoint boundaries, no reload.");
    return {.attempts_per_checkpoint = run.policy().attempts_per_checkpoint, .boundary_reload = StoryRunPolicy::BoundaryReload::STRAIGHT_THROUGH};
}

} // namespace

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const StoryData story_data = load_run_story(env.console);
    const std::vector<StoryCheckpoint> rows = panel_rows(story_data);
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= rows.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    connect_controller(env.console, context, rows[first]);

    StoryEnv story{
        .env = env, .context = context, .new_game = new_game_choices(env.console, story_row_launches_game(rows[first])),
        .language = RUN.language(), .battle_ai = BATTLE_AI
    };
    PlzaConsoleBackend backend(story);
    const StoryGame<ZonePose> game = run_game(story, STARTER, story_data, RUN.breakpoints(), context);
    StoryEngine<ZonePose> engine(game, backend);
    RunState state;
    load_start_save(env.console, story, backend, rows, first, START_FROM_BACKUP_SAVE);

    if (const std::optional<VerifyPolicy> verify = RUN.VERIFY.policy()){
        const VerifyReport report = engine.verify_checkpoint(rows[first], *verify, state);
        if (!report.verified){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                std::format(
                    "AutoStory verify: {} {}/{} passed, {} needed{}", report.checkpoint, report.passes, report.runs,
                    verify->passes_needed, report.failure.succeeded() ? std::string() : std::format(": {}", to_string(report.failure))
                )
            );
        }
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
        return;
    }
    const std::vector<StoryCheckpoint> checkpoints = story_run_rows(rows, first, last);
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
