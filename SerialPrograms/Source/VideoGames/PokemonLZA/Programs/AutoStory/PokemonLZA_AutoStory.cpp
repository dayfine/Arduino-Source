/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
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
        "Play the main story of Pokemon Legends: Z-A. Prototype: plays a new game to the station platform, as recorded on a console. "
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
    , RELOAD_AT_BOUNDARIES(
        "<b>Reload at Checkpoint Boundaries:</b><br>Reload the save before a checkpoint whose plan asks for it "
        "when the run comes from the checkpoint before. Off: run straight on. A failed checkpoint's retry reloads either way.",
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
    PA_ADD_OPTION(LANGUAGE);
    RUN.LANGUAGE.set_visibility(ConfigOptionState::HIDDEN);
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(RELOAD_AT_BOUNDARIES);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

namespace{

//  Checkpoints after the new game start from a save (01.01 from the
//  platform autosave), so the run loads it first instead of assuming the
//  game is already there. 01.00 starts without one.
void load_start_save(
    ConsoleHandle& console, const StoryGame<ZonePose>& game, StoryBackend<ZonePose>& backend,
    const std::vector<StoryCheckpoint>& rows, size_t first
){
    if (story_row_starts_without_save(rows[first])){
        return;
    }
    const Outcome loaded = game.reload(backend);
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
    if (story_row_starts_without_save(first_row)){
        require_confirm_choices(console, context);
    }
    require_player(console, context, BUTTON_L);
}

//  PLZA's game for a run on `story`, pausing at `breakpoints` (the
//  BREAKPOINTS option): before each step, or each leg too, where a lab build
//  waits for the user (Pokemon_StoryBreakpoints.h).
StoryGame<ZonePose> run_game(StoryEnv& story, const StoryData& data, StoryBreakpoints breakpoints, CancellableScope& scope){
    StoryGame<ZonePose> game = make_plza_story_game(story, data.autosaves);
    game.breakpoints = breakpoints;
    game.pause_point = make_story_pause_hook(scope);
    return game;
}

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
    const std::vector<StoryCheckpoint>& rows = story_data.checkpoints;
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= rows.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    connect_controller(env.console, context, rows[first]);

    StoryEnv story{
        .env = env, .context = context, .language = LANGUAGE, .battle_ai = BATTLE_AI
    };
    PlzaConsoleBackend backend(story);
    const StoryGame<ZonePose> game = run_game(story, story_data, RUN.breakpoints(), context);
    StoryEngine<ZonePose> engine(game, backend);
    RunState state;
    load_start_save(env.console, game, backend, rows, first);

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
