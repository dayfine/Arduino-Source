/*  AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/Navigation/Pokemon_StorySegments.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryEvents.h"
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

//  The start and end dropdowns' entries: the table's checkpoints, 00.01 to
//  00.09. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = Pokemon::story_checkpoint_database(story_checkpoints());
    return database;
}

}

AutoStory::AutoStory()
    : RUN(checkpoint_database())
    , NEW_GAME_FROM_TITLE(
        "<b>New game from the title screen:</b><br>The game is on its title screen with no save data. "
        "Needed to start at 00.01. Otherwise the program loads the save, which must be the one the start checkpoint begins from.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(NEW_GAME_FROM_TITLE);
    PA_ADD_OPTION(NOTIFICATIONS);
}


//  The save loaded must be the one checkpoint `first` starts from: one left
//  by an earlier checkpoint (a run that failed before saving) would start
//  it in the wrong place (console B, 2026-11-01: 00.09 on the forest's
//  save). Checked on the SAVE card; a card that can't be read is let pass.
static void check_save_place(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, Language language
){
    const StoryCheckpoint& checkpoint = checkpoints[first];
    const std::string expected = checkpoint.start ? save_card_place(checkpoint.start->map) : "";
    if (expected.empty()){
        return;
    }
    //  The START menu as the checkpoint before left it.
    const std::string place = read_save_place(env.console, context, checkpoints[first - 1].menu, language);
    if (place.empty()){
        env.console.log("Couldn't read the save card's place; starting " + checkpoint.id + " anyway.", COLOR_ORANGE);
        return;
    }
    if (place != expected){
        throw UserSetupError(
            env.console,
            "The save is in " + place + ", but checkpoint " + checkpoint.id + " starts in " + expected +
            " (" + checkpoint.start->map + "). Start at the checkpoint the save is for."
        );
    }
    env.console.log("The save is in " + place + ", where " + checkpoint.id + " starts.");
}

namespace{

//  A failed attempt reloads the last save, with the supplies it had; the
//  engine's lines go to the console's log.
Pokemon::SegmentHooks story_hooks(SingleSwitchProgramEnvironment& env, StoryEnv& story){
    Pokemon::SegmentHooks hooks;
    hooks.reload = [&story](Pokemon::RunState&){
        if (!story.has_save){
            return Pokemon::outcome_failed("no_save", "nothing saved yet");
        }
        try{
            soft_reset(story.console, story.context);
        }catch (OperationFailedException& e){
            return Pokemon::outcome_failed("reload_failed", e.message());
        }
        story.supplies = story.saved_supplies;
        return Pokemon::outcome_ok();
    };
    hooks.log = [&env](const std::string& line){
        env.console.log(line);
    };
    return hooks;
}

//  The shared AutoStory stats for one run: a segment, the checkpoints it
//  passed and the reloads after failed attempts.
void count_run(SingleSwitchProgramEnvironment& env, const Pokemon::SegmentReport& report){
    Pokemon::AutoStoryStats& stats = env.current_stats<Pokemon::AutoStoryStats>();
    stats.m_segment++;
    for (const Pokemon::CheckpointRun& run : report.runs){
        stats.m_checkpoint += run.passed ? 1 : 0;
    }
    stats.m_reset += report.reloads;
    env.update_stats();
}

}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= checkpoints.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    if (first == 0 && !NEW_GAME_FROM_TITLE){
        throw UserSetupError(env.console, "Checkpoint 00.01 starts a new game: start on the title screen with no save.");
    }

    StoryEnv story{env.console, context, checkpoints[first].supplies, checkpoints[first].supplies, false, RUN.checks()};
    if (!(first == 0 && NEW_GAME_FROM_TITLE)){
        env.console.log("Loading the save for " + checkpoints[first].id + ".");
        soft_reset(env.console, context);
        story.has_save = true;
        check_save_place(env, context, checkpoints, first, story.language);
    }

    StoryExecutor executor(story);
    executor.set_checks(RUN.checks());
    executor.set_language(RUN.language());
    Pokemon::Segment segment = make_story_segment(checkpoints, executor);
    Pokemon::SegmentHooks hooks = story_hooks(env, story);

    Pokemon::RunState state;
    const Pokemon::SegmentReport report = run_segment(
        state, segment, Pokemon::SegmentRange{checkpoints[first].id, checkpoints[last].id}, RUN.policy(), hooks
    );
    count_run(env, report);
    if (!report.completed){
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
