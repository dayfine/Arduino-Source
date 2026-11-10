/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;


AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        STRING_POKEMON + " LZA", "Auto Story",
        "Programs/PokemonLZA/AutoStory.html",
        "Play the main story of Pokemon Legends: Z-A. Prototype: Main Missions 01-03 as recorded on a console; plays from a new game to Wild Zone 1, the rest waits for code.",
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

//  The engine, PLZA's game on the console and the run's state.
struct StoryRun{
    PlzaConsoleBackend& backend;
    const StoryGame<ZonePose>& game;
    StoryEngine<ZonePose>& engine;
    RunState state = {};
    StoryRunCounts counts = {.segments = 1, .checkpoints = 0, .resets = 0};
};

//  Back to the save the game is on (game.reload); counted.
Outcome reload(StoryRun& run){
    run.counts.resets++;
    return run.game.reload(run.backend);
}

//  `checkpoint` until it passes, `attempts` times at most, reloading
//  between attempts. Only the first checkpoint of a run is retried: the
//  save a reload returns to is the one it started from, since the engine
//  saves only as each checkpoint's phase allows. A failure's report.
StoryEngineReport run_with_retries(
    StoryRun& run, const StoryCheckpoint& checkpoint, SavePoint point, size_t attempts
){
    StoryEngineReport report;
    for (size_t attempt = 1; attempt <= attempts; attempt++){
        if (attempt > 1){
            run.backend.log(std::format("[Segment] {}: reloading for attempt {}/{}", checkpoint.id, attempt, attempts));
            const Outcome reloaded = reload(run);
            if (!reloaded.succeeded()){
                report.run.failure = reloaded;
                return report;
            }
        }
        report = run.engine.run_checkpoint(checkpoint, run.state, point);
        if (report.verdict == CheckpointVerdict::PASSED){
            run.counts.checkpoints++;
            return report;
        }
    }
    return report;
}

//  Verify mode: the checkpoint as many times as asked, from its save; the
//  last run may save (AFTER_VERIFICATION, from its phase).
void verify_story_checkpoint(
    SingleSwitchProgramEnvironment& env, StoryRun& run, const StoryCheckpoint& checkpoint, const VerifyPolicy& policy
){
    size_t passes = 0;
    for (size_t c = 0; c < policy.runs; c++){
        if (c > 0 && !reload(run).succeeded()){
            throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "AutoStory verify: the reload failed.");
        }
        const SavePoint point = c + 1 == policy.runs ? SavePoint::RUN_END : SavePoint::CHECKPOINT_END;
        if (run.engine.run_checkpoint(checkpoint, run.state, point).verdict == CheckpointVerdict::PASSED){
            passes++;
        }
        env.console.log(std::format("[Verify] {}: run {}/{}, {} passed", checkpoint.id, c + 1, policy.runs, passes));
    }
    const std::string summary = std::format(
        "AutoStory verify: {} {}/{} passed, {} needed.", checkpoint.id, passes, policy.runs, policy.passes_needed
    );
    if (passes < policy.passes_needed){
        throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, summary);
    }
    env.console.log(summary);
}

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
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= rows.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    require_player(env.console, context, BUTTON_L);

    StoryEnv story{.env = env, .context = context, .language = RUN.language(), .battle_ai = BATTLE_AI};
    PlzaConsoleBackend backend(story);
    const StoryGame<ZonePose> game = make_plza_story_game(backend, STARTER);
    StoryEngine<ZonePose> engine(game, backend);
    StoryRun run{.backend = backend, .game = game, .engine = engine};

    if (const std::optional<VerifyPolicy> verify = RUN.VERIFY.policy()){
        verify_story_checkpoint(env, run, rows[first], *verify);
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
        return;
    }
    const size_t attempts = RUN.policy().attempts_per_checkpoint;
    for (size_t c = first; c <= last; c++){
        const SavePoint point = c == last ? SavePoint::RUN_END : SavePoint::CHECKPOINT_END;
        const StoryEngineReport report = run_with_retries(run, rows[c], point, c == first ? attempts : 1);
        if (report.verdict != CheckpointVerdict::PASSED){
            record_story_counts(env.current_stats<AutoStoryStats>(), run.counts);
            env.update_stats();
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                std::format(
                    "AutoStory stopped at {} {}: {}", rows[c].id, report.run.failed_name,
                    report.run.completed ? std::string("postconditions failed") : to_string(report.run.failure)
                )
            );
        }
    }
    record_story_counts(env.current_stats<AutoStoryStats>(), run.counts);
    env.update_stats();
    env.console.log("AutoStory: done through " + rows[last].id + ".");
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
