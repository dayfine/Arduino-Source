/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "PokemonLZA_AutoStoryActions.h"
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
        "Play the main story of Pokemon Legends: Z-A. Prototype: Mission 1 only, not run on a console yet.",
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
    static const StringSelectDatabase database = story_checkpoint_database(story_checkpoints());
    return database;
}

//  Each row saves at its end; a failed attempt resets the game from HOME,
//  which loads the save the row before it made.
SegmentHooks story_hooks(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    SegmentHooks hooks;
    hooks.save = [&env, &context](RunState&){
        save_story_game(env.console, context);
        return outcome_ok();
    };
    hooks.reload = [&env, &context](RunState&){
        go_home(env.console, context);
        if (!reset_game_from_home(env, env.console, context)){
            return outcome_failed("reload_failed", "Failed to reset the game from HOME.");
        }
        return outcome_ok();
    };
    hooks.log = [&env](const std::string& line){
        env.log(line, COLOR_BLUE);
    };
    hooks.status = [&env](const std::string& line){
        env.console.overlay().add_log(line);
    };
    return hooks;
}

//  Verify mode: the start checkpoint as many times as asked, from its save.
void verify_story_checkpoint(
    SingleSwitchProgramEnvironment& env, RunState& state, Segment& segment, SegmentHooks& hooks,
    const std::string& id, const VerifyPolicy& policy
){
    const VerifyReport report = verify_checkpoint(state, segment, id, policy, hooks);
    const std::string summary = std::format(
        "AutoStory verify: {} {}/{} passed, {} needed.", id, report.passes, policy.runs, policy.passes_needed
    );
    if (!report.verified){
        throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, summary);
    }
    env.console.log(summary);
}

} // namespace


AutoStory::AutoStory()
    : RUN(checkpoint_database())
    , STARTER(
        "<b>Starter Pokemon:</b>",
        {
            {StarterChoice::CHIKORITA, "chikorita", "Chikorita"},
            {StarterChoice::TEPIG,     "tepig",     "Tepig"},
            {StarterChoice::TOTODILE,  "totodile",  "Totodile"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StarterChoice::TEPIG
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

    StoryEnv story{.env = env, .context = context, .starter = STARTER, .battle_ai = BATTLE_AI};
    StoryExecutor executor(story);
    executor.set_checks(RUN.checks());
    executor.set_language(RUN.language());
    Segment segment = make_story_segment(rows, executor);
    SegmentHooks hooks = story_hooks(env, context);
    RunState state;

    if (const std::optional<VerifyPolicy> verify = RUN.VERIFY.policy()){
        verify_story_checkpoint(env, state, segment, hooks, rows[first].id, *verify);
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
        return;
    }
    const SegmentReport report = run_segment(state, segment, SegmentRange{rows[first].id, rows[last].id}, RUN.policy(), hooks);
    record_story_stats(env.current_stats<AutoStoryStats>(), report);
    env.update_stats();
    if (!report.completed){
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("AutoStory stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, to_string(report.failure))
        );
    }
    env.console.log("AutoStory: done through " + rows[last].id + ".");
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
