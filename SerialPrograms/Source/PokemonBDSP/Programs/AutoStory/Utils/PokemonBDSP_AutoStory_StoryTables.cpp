/*  BDSP AutoStory - Story tables
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameEntry.h"
#include "PokemonBDSP_AutoStory_StoryTables.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Checkpoint;
using Pokemon::Event;
using Pokemon::Outcome;
using Pokemon::RunState;


namespace{

//  The attempts checkpoint_reattempt_loop() gives a checkpoint.
constexpr size_t STORY_ATTEMPTS = 20;

Event save_event(StoryStepRunner& runner, const std::string& id){
    Event event;
    event.name = "save";
    event.why = "the save a failed step resets to";
    event.skip_if.push_back(Pokemon::condition("saved at the start", [&runner, id](RunState&){
        return runner.saved_at_start(id);
    }));
    event.act = [&runner, id](RunState&){
        return runner.save_at_start(id);
    };
    return event;
}

Event step_event(StoryStepRunner& runner, const std::string& id, StoryFailure on_failure, StoryStepSpec step){
    Event event;
    event.name = std::move(step.name);
    event.why = std::move(step.why);
    event.act = [&runner, id, on_failure, run = std::move(step.step)](RunState&){
        return runner.run_step(id, on_failure, run);
    };
    return event;
}

}


Pokemon::CheckpointSpec story_checkpoint(StoryStepRunner& runner, StoryCheckpointSpec spec){
    Pokemon::CheckpointSpec row;
    row.id = spec.id;
    row.title = spec.title;
    row.build = [&runner, spec = std::move(spec)](){
        Checkpoint checkpoint;
        checkpoint.id = spec.id;
        if (spec.on_failure == StoryFailure::RELOAD_AND_RETRY){
            checkpoint.events.push_back(save_event(runner, spec.id));
        }
        for (const StoryStepSpec& step : spec.steps){
            checkpoint.events.push_back(step_event(runner, spec.id, spec.on_failure, step));
        }
        return checkpoint;
    };
    return row;
}


Pokemon::Segment story_segment(StoryStepRunner& runner, std::string id, std::vector<StoryCheckpointSpec> rows){
    Pokemon::Segment segment;
    segment.id = std::move(id);
    for (StoryCheckpointSpec& row : rows){
        segment.checkpoints.push_back(story_checkpoint(runner, std::move(row)));
    }
    return segment;
}


ConsoleStoryRunner::ConsoleStoryRunner(StoryConsole console)
    : m_console(console)
{}

bool ConsoleStoryRunner::saved_at_start(const std::string& id) const{
    return m_saved.contains(id);
}

//  A save that fails is a failed first attempt, as in the loop: the retry
//  resets and does not save again.
Outcome ConsoleStoryRunner::save_at_start(const std::string& id){
    m_saved.insert(id);
    return run_step(id, StoryFailure::RELOAD_AND_RETRY, [](StoryConsole& console){
        checkpoint_save(console.env, console.context, console.options.notif_status_update, console.stats, console.options.checkpoint_save_mode);
        console.stats.m_first_run = false;
    });
}

Outcome ConsoleStoryRunner::run_step(const std::string& id, StoryFailure on_failure, const StoryStep& step){
    if (on_failure == StoryFailure::STOP){
        step(m_console);
        return Pokemon::outcome_ok();
    }
    SingleSwitchProgramEnvironment& env = m_console.env;
    try{
        step(m_console);
        return Pokemon::outcome_ok();
    }catch (OperationFailedException& e){
        //  The fake-save mode's loop around the whole range retries it.
        if (m_console.options.checkpoint_retry_scope == CheckpointRetryScope::DEFER_TO_SELECTED_RANGE){
            throw;
        }
        const size_t attempt = m_failures[id]++;
        //  The lab's run watcher reads this line as a failed attempt.
        env.console.log(std::format("checkpoint_reattempt_loop: attempt {} failed: {}: {}", attempt, id, e.message()), COLOR_RED);
        //  Keep what the screen showed at the failure; the retry resets the game.
        if (VideoSnapshot screen = env.console.video().snapshot()){
            const std::string folder = SCREENSHOTS_PATH() + "AutoStory/failures/";
            Filesystem::create_directories(Filesystem::Path(folder));
            const std::string path = std::format("{}{}-attempt{}.png", folder, now_to_filestring(), attempt);
            screen.frame->save(path);
            env.console.log("Failure screenshot: " + path);
        }
        m_console.stats.m_reset++;
        env.update_stats();
        if (attempt + 1 >= STORY_ATTEMPTS){
            throw;
        }
        if (g_stop_at_first_failure){
            env.console.log("Stopping at the first failure; the game stays where it failed.", COLOR_ORANGE);
            throw;
        }
        return Pokemon::outcome_failed("operation_failed", e.message());
    }
}

Outcome ConsoleStoryRunner::reload(){
    SingleSwitchProgramEnvironment& env = m_console.env;
    env.console.log("Resetting game from HOME before retry...", COLOR_ORANGE);
    if (!reset_game_from_home(env, env.console, m_console.context, true, 1000ms)){
        throw OperationFailedExceptionWithScreenshot(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Failed to reset game from HOME during checkpoint retry.",
            env.console
        );
    }
    return Pokemon::outcome_ok();
}

void ConsoleStoryRunner::run(Pokemon::Segment& segment, const Pokemon::SegmentRange& range){
    SingleSwitchProgramEnvironment& env = m_console.env;
    Pokemon::SegmentHooks hooks;
    hooks.save = [](RunState&){
        return Pokemon::outcome_failed("no_save_hook", "story checkpoints save at their start");
    };
    hooks.reload = [this](RunState&){
        return reload();
    };
    hooks.log = [&env](const std::string& line){
        env.log(line, COLOR_BLUE);
    };
    Pokemon::SegmentPolicy policy;
    policy.attempts_per_checkpoint = STORY_ATTEMPTS;
    RunState state;
    const Pokemon::SegmentReport report = Pokemon::run_segment(state, segment, range, policy, hooks);
    if (!report.completed){
        throw OperationFailedExceptionWithScreenshot(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format(
                "AutoStory segment {}: failed at {} {}: {}",
                segment.id, report.failed_checkpoint, report.failed_event, Pokemon::to_string(report.failure)
            ),
            env.console
        );
    }
}


void run_story_table(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const AutoStoryOptions& options, AutoStoryStats& stats,
    const StorySegmentFactory& make_segment, const Pokemon::SegmentRange& range
){
    ConsoleStoryRunner runner(StoryConsole{env, context, options, stats});
    Pokemon::Segment segment = make_segment(runner);
    runner.run(segment, range);
}



}
}
}
