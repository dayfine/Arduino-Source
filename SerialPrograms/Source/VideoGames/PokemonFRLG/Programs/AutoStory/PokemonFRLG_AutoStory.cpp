/*  AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Pokemon_Strings.h"
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
//  00.10. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const std::vector<std::string> labels = []{
        std::vector<std::string> result;
        for (const StoryCheckpoint& checkpoint : story_checkpoints()){
            result.push_back(Pokemon::story_checkpoint_label(checkpoint));
        }
        return result;
    }();
    static const StringSelectDatabase database = Pokemon::story_checkpoint_database(labels);
    return database;
}

const std::string KEEP_START_FRAME_TEXT =
    "<b>Keep the start frame:</b><br>Off by default. The save loaded was made by hand where the start checkpoint "
    "begins: keep the screen after loading it as that checkpoint's start frame, which later runs compare their "
    "save with. The program keeps one by itself at each save it makes.";

} // namespace

AutoStory::AutoStory()
    : RUN(checkpoint_database(), Pokemon::FireRedLeafGreenLanguages_Database())
    , NEW_GAME_FROM_TITLE(
        "<b>New game from the title screen:</b><br>The game is on its title screen with no save data. "
        "Needed to start at 00.01. Otherwise the program loads the save, which must be the one the start checkpoint begins from.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , KEEP_START_FRAME(KEEP_START_FRAME_TEXT, LockMode::LOCK_WHILE_RUNNING, false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(NEW_GAME_FROM_TITLE);
    PA_ADD_OPTION(KEEP_START_FRAME);
    PA_ADD_OPTION(NOTIFICATIONS);
}


//  The save loaded must be the one checkpoint `first` starts from: one left
//  by an earlier checkpoint (a run that failed before saving) would start
//  it in the wrong place (console B, 2026-11-01: 00.09 on the forest's
//  save). Checked on the SAVE card; a card that can't be read is let pass.
static void check_start_save(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, const StoryCheckpoint& checkpoint
){
    if (!checkpoint.start){
        return;
    }
    const std::string error = check_save_place(story, Pokemon::grid_pose_of(*checkpoint.start).map);
    if (!error.empty()){
        throw UserSetupError(env.console, error + " Start at the checkpoint " + checkpoint.id + " the save is for.");
    }
}

namespace{

//  The story's state for a run from `checkpoint`: its supplies, the checks
//  level, and the program's start frames, the first start check keeping
//  the screen as its frame if `keep_start_frame` (a save made by hand).
StoryEnv story_env(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StoryCheckpoint& checkpoint, Pokemon::StoryCheckLevel checks, bool keep_start_frame
){
    StoryEnv story{env.console, context, checkpoint.supplies, checkpoint.supplies, false, checks};
    story.start_frames = program_start_frames();
    story.keep_start_frame = keep_start_frame;
    return story;
}

//  A failed attempt reloads the last save, with the supplies it had, read
//  from its bag again; the engine's lines go to the console's log.
Pokemon::SegmentHooks story_hooks(SingleSwitchProgramEnvironment& env, StoryEnv& story){
    Pokemon::SegmentHooks hooks;
    hooks.reload = [&story](Pokemon::RunState&){
        if (!story.has_save){
            return Pokemon::outcome_failed("no_save", "nothing saved yet");
        }
        //  The save reloaded must be where the checkpoint starts: a save
        //  that didn't happen leaves an older one (console B, 2026-11-02:
        //  00.02 walked the bedroom's route in Pewter City).
        try{
            soft_reset(story.console, story.context);
            const std::string error = check_save_place(story, story.start_map);
            if (!error.empty()){
                return Pokemon::outcome_failed("wrong_save", error);
            }
            story.supplies = story.saved_supplies;
            //  Every reload, so each verify run starts from the bag as the
            //  save holds it, not from a count once read wrong or not at
            //  all (console B, 2026-11-03: 0 Potions in all 3 runs).
            read_bag_supplies(story, story.start_menu);
        }catch (OperationFailedException& e){
            return Pokemon::outcome_failed("reload_failed", e.message());
        }
        return Pokemon::outcome_ok();
    };
    hooks.log = [&env](const std::string& line){
        env.console.log(line);
    };
    //  The checkpoint and the step under way, for whoever watches the stream.
    hooks.status = [&env](const std::string& line){
        env.console.overlay().add_log(line);
    };
    return hooks;
}

//  The game on the save checkpoint `checkpoint` starts from, with the
//  supplies and moves read from it.
void load_verify_save(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context, StoryEnv& story, const StoryCheckpoint& checkpoint
){
    if (!checkpoint.start){
        throw UserSetupError(env.console,
            "Verify mode reloads the checkpoint's save between runs: " + checkpoint.id + " starts a new game and has none."
        );
    }
    env.console.log("Verify mode: loading the save for " + checkpoint.id + ".");
    soft_reset(env.console, context);
    story.has_save = true;
    check_start_save(env, story, checkpoint);
    read_lead_moves(story, checkpoint.menu);
    read_bag_supplies(story, checkpoint.menu);
}

//  Verify mode: the start checkpoint run from its save as `policy` says,
//  never saving at its end, so every run starts from the same save.
//  Throws unless enough runs pass.
void verify_story_checkpoint(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const Pokemon::StoryRunOptions& run, const Pokemon::VerifyPolicy& policy, EventNotificationOption& finished
){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const StoryCheckpoint& checkpoint = checkpoints[run.start_index()];
    StoryEnv story = story_env(env, context, checkpoint, run.check_level(), false);
    load_verify_save(env, context, story, checkpoint);
    story.language = run.language();
    StoryExecutor executor(story);
    Pokemon::Segment segment = make_story_segment(checkpoints, executor);
    Pokemon::SegmentHooks hooks = story_hooks(env, story);

    Pokemon::RunState state;
    const Pokemon::VerifyReport report = Pokemon::verify_checkpoint(checkpoint.id, policy, segment, hooks, state);
    const std::string summary = std::format(
        "AutoStory verify: {} {}/{} passed, {} needed.", checkpoint.id, report.passes, policy.runs, policy.passes_needed
    );
    if (!report.verified){
        throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, summary);
    }
    env.console.log(summary);
    send_program_finished_notification(env, finished, summary);
}


} // namespace

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    if (const std::optional<Pokemon::VerifyPolicy> verify = RUN.VERIFY.policy()){
        verify_story_checkpoint(env, context, RUN, *verify, NOTIFICATION_PROGRAM_FINISH);
        return;
    }
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= checkpoints.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    if (first == 0 && !NEW_GAME_FROM_TITLE){
        throw UserSetupError(env.console, "Checkpoint 00.01 starts a new game: start on the title screen with no save.");
    }

    StoryEnv story = story_env(env, context, checkpoints[first], RUN.check_level(), KEEP_START_FRAME);
    if (!(first == 0 && NEW_GAME_FROM_TITLE)){
        env.console.log("Loading the save for " + checkpoints[first].id + ".");
        soft_reset(env.console, context);
        story.has_save = true;
        check_start_save(env, story, checkpoints[first]);
        read_lead_moves(story, checkpoints[first].menu);
        read_bag_supplies(story, checkpoints[first].menu);
    }

    story.language = RUN.language();
    StoryExecutor executor(story);
    Pokemon::Segment segment = make_story_segment(checkpoints, executor);
    Pokemon::SegmentHooks hooks = story_hooks(env, story);

    Pokemon::RunState state;
    const Pokemon::SegmentReport report = run_segment(
        Pokemon::SegmentRange{checkpoints[first].id, checkpoints[last].id}, RUN.policy(), segment, hooks, state
    );
    Pokemon::record_story_counts(report.story_counts(), env.current_stats<Pokemon::AutoStoryStats>());
    env.update_stats();
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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
