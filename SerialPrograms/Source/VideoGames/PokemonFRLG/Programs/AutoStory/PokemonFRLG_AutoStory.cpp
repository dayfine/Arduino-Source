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

AutoStory::AutoStory()
    : START_CHECKPOINT(
        "<b>Start at checkpoint 00.0N:</b><br>1: new game, 2: bedroom, 3: Pallet Town, 4: Oak's Lab, "
        "5: after the rival's battle, 6: Viridian Mart, 7: Pokedex, 8: Pewter Center.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 8, 1, 1
    )
    , END_CHECKPOINT(
        "<b>End after checkpoint 00.0N:</b><br>8 ends with the Boulder Badge.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 8, 8, 8
    )
    , NEW_GAME_FROM_TITLE(
        "<b>New game from the title screen:</b><br>The game is on its title screen with no save data. "
        "Needed to start at 1. Otherwise the program loads the save, which must be the one the start checkpoint begins from.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , ATTEMPTS_PER_CHECKPOINT(
        "<b>Attempts per checkpoint:</b><br>A failed checkpoint reloads its save and runs again, up to this many times in all.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 5, 2, 2
    )
    , CHECKS(
        "<b>Checks:</b><br>Thorough reads the screen before acting and confirms after: the item on a Mart row "
        "before buying it, the bag's Items pocket after shopping (its counts are the ones kept), and an item's name "
        "in the bag before using it. Fast trusts the counts it keeps and the timings.",
        {
            {StoryChecks::THOROUGH, "thorough", "Thorough"},
            {StoryChecks::FAST, "fast", "Fast"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StoryChecks::THOROUGH
    )
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(START_CHECKPOINT);
    PA_ADD_OPTION(END_CHECKPOINT);
    PA_ADD_OPTION(NEW_GAME_FROM_TITLE);
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(CHECKS);
    PA_ADD_OPTION(NOTIFICATIONS);
}


void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t first = START_CHECKPOINT - 1;
    const size_t last = END_CHECKPOINT - 1;
    if (first > last || last >= checkpoints.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    if (first == 0 && !NEW_GAME_FROM_TITLE){
        throw UserSetupError(env.console, "Checkpoint 00.01 starts a new game: start on the title screen with no save.");
    }

    StoryEnv story{env.console, context, checkpoints[first].supplies, checkpoints[first].supplies, false, CHECKS};
    if (!(first == 0 && NEW_GAME_FROM_TITLE)){
        env.console.log("Loading the save for " + checkpoints[first].id + ".");
        soft_reset(env.console, context);
        story.has_save = true;
    }

    Pokemon::Segment segment = make_story_segment(checkpoints, story);
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
    Pokemon::SegmentPolicy policy;
    policy.attempts_per_checkpoint = ATTEMPTS_PER_CHECKPOINT;

    Pokemon::RunState state;
    const Pokemon::SegmentReport report = run_segment(
        state, segment, Pokemon::SegmentRange{checkpoints[first].id, checkpoints[last].id}, policy, hooks
    );
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
