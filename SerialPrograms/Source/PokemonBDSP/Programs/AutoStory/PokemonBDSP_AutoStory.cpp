/*  BDSP AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include <format>
#include <set>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/Navigation/Pokemon_StorySegments.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameEntry.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
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
    static const StringSelectDatabase database = Pokemon::story_checkpoint_database(story_checkpoints());
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

//  A failed attempt resets the game from HOME, which loads the save the
//  row made at its start; the engine's lines go to the console's log.
Pokemon::SegmentHooks story_hooks(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    Pokemon::SegmentHooks hooks;
    hooks.save = [](Pokemon::RunState&){
        return Pokemon::outcome_failed("no_save_hook", "story rows save at their start");
    };
    hooks.reload = [&env, &context](Pokemon::RunState&){
        env.console.log("Resetting game from HOME before retry...", COLOR_ORANGE);
        if (!reset_game_from_home(env, env.console, context, true, 1000ms)){
            return Pokemon::outcome_failed("reload_failed", "Failed to reset the game from HOME.");
        }
        return Pokemon::outcome_ok();
    };
    hooks.log = [&env](const std::string& line){
        env.log(line, COLOR_BLUE);
    };
    return hooks;
}

//  Leave the Switch in a known state after a failure: on HOME with the
//  game closed, so the next run (or the user) starts from there.
void close_game_after_failure(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("AutoStory failed: going to HOME and closing the game.", COLOR_RED);
    try{
        go_home(env.console, context);
        close_game_from_home(env.console, context);
    }catch (OperationFailedException& e){
        env.log("Could not close the game: " + e.message(), COLOR_RED);
    }
}

}


BDSPAutoStory::BDSPAutoStory()
    : RUN(checkpoint_database())
    , NEW_GAME_INTRO(
        "<b>Start a new game first:</b><br>"
        "For a game with no save: restart it and play the intro (language, appearance, names) "
        "until the player is in control in the bedroom. Only when starting at 00.00.",
        LockMode::LOCK_WHILE_RUNNING, false
    )
    , PLAYER_NAME(false, "<b>Player name:</b><br>For a new game. Letters and digits.", LockMode::LOCK_WHILE_RUNNING, "Kosmos", "Kosmos")
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
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void BDSPAutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= checkpoints.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    //  The reads that use text (Location.h) take the game's language from here.
    g_story_language = RUN.language();

    StoryEnv story{env, context, STARTER, PLAYER_NAME};
    StoryExecutor executor(story);
    executor.set_checks(RUN.checks());
    executor.set_language(RUN.language());
    executor.set_stop_rows(rows_that_stop(checkpoints));
    std::set<std::string> saved;
    Pokemon::Segment segment = make_story_segment(checkpoints, executor, saved);
    Pokemon::SegmentHooks hooks = story_hooks(env, context);

    Pokemon::RunState state;
    if (!(first == 0 && NEW_GAME_INTRO)){
        state.flags.insert("save_loaded");
    }
    Pokemon::SegmentReport report;
    try{
        report = run_segment(state, segment, Pokemon::SegmentRange{checkpoints[first].id, checkpoints[last].id}, RUN.policy(), hooks);
    }catch (OperationFailedException&){
        if (!RUN.STOP_AT_FIRST_FAILURE){
            close_game_after_failure(env, context);
        }
        throw;
    }
    Pokemon::AutoStoryStats& stats = env.current_stats<Pokemon::AutoStoryStats>();
    Pokemon::record_story_stats(stats, report);
    env.update_stats();
    if (!report.completed){
        if (!report.stopped_at_first_failure){
            close_game_after_failure(env, context);
        }
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format(
                "AutoStory stopped at {} {}: {}",
                report.failed_checkpoint, report.failed_event, Pokemon::to_string(report.failure)
            )
        );
    }
    env.console.log("AutoStory: done through " + checkpoints[last].id + ".");
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}



}
}
}
