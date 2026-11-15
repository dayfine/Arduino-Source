/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_CheckpointAttempts.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon_StoryEngine.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  A failure every attempt would repeat: no reload for it.
bool repeats_on_reload(const Outcome& failure){
    return failure.failure == GAME_NOT_RUNNING || failure.failure == CONSOLE_ONLY || failure.failure == "lint_failed";
}

} // namespace


template <StoryPose PoseType>
StoryEngine<PoseType>::StoryEngine(const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend)
    : m_game(game)
    , m_backend(backend)
    , m_registry(game)
    , m_runner(m_registry, backend)
{}

template <StoryPose PoseType>
StoryEngineReport StoryEngine<PoseType>::run_checkpoint(const StoryCheckpoint& checkpoint, RunState& state){
    StoryEngineReport report;
    const std::vector<std::string> problems = m_registry.lint(checkpoint);
    if (!problems.empty()){
        for (const std::string& problem : problems){
            m_backend.log(std::format("[Lint] {}", problem));
        }
        report.run.failure = outcome_failed("lint_failed", problems.front());
        return report;
    }
    report.run = run_steps(checkpoint, state);
    if (!report.run.completed){
        m_backend.log(std::format("[Save] {}: run failed: not saved", checkpoint.id));
        return report;
    }
    report.passed = true;
    save_if_passed(checkpoint, report);
    return report;
}

template <StoryPose PoseType>
Outcome StoryEngine<PoseType>::reload(RunState& state){
    state = RunState{};
    if (!m_game.reload){
        return outcome_failed("reload_failed", "the game has no reload");
    }
    const Outcome reloaded = m_game.reload(m_backend);
    if (!reloaded.succeeded()){
        return outcome_failed("reload_failed", to_string(reloaded));
    }
    if (reloaded.pose){
        state.pose = *reloaded.pose;
    }
    return outcome_ok();
}

template <StoryPose PoseType>
StoryEngineReport StoryEngine<PoseType>::run_with_retries(
    const StoryCheckpoint& checkpoint, size_t attempts, RunState& state, CheckpointRun& run
){
    StoryEngineReport report;
    const CheckpointAttempts loop{
        .max_attempts = attempts,
        .run = [&](size_t attempt){
            run.attempts = attempt;
            report = run_checkpoint(checkpoint, state);
            if (report.passed){
                return AttemptResult::PASSED;
            }
            return repeats_on_reload(report.run.failure) ? AttemptResult::FINAL : AttemptResult::FAILED;
        },
        .reset = [&](size_t next_attempt){
            m_backend.log(checkpoint_reset_line(checkpoint.id, next_attempt, attempts));
            const Outcome reloaded = reload(state);
            if (!reloaded.succeeded()){
                report.run.completed = false;
                report.run.failed_name.clear();
                report.run.failure = reloaded;
                return false;
            }
            return true;
        },
        .log = [this](const std::string& line){
            m_backend.log(line);
        },
    };
    run.passed = run_checkpoint_attempts(checkpoint.id, loop).passed;
    return report;
}

template <StoryPose PoseType>
SegmentReport StoryEngine<PoseType>::run_segment(
    std::string_view id, std::span<const StoryCheckpoint> checkpoints, size_t attempts_per_checkpoint,
    RunState& state
){
    SegmentReport report;
    m_backend.log(std::format("[Segment] {}: start, {} checkpoints", id, checkpoints.size()));
    //  Whether a reload returns to the save the next checkpoint starts from.
    bool start_saved = true;
    for (const StoryCheckpoint& checkpoint : checkpoints){
        const size_t attempts = start_saved ? attempts_per_checkpoint : 1;
        if (!start_saved && attempts_per_checkpoint > 1){
            m_backend.log(std::format("[Segment] {}: {}: no save at its start, so no reload if it fails", id, checkpoint.id));
        }
        report.checkpoints_run++;
        report.runs.push_back(CheckpointRun{.id = checkpoint.id});
        const StoryEngineReport result = run_with_retries(checkpoint, attempts, state, report.runs.back());
        report.reloads += report.runs.back().attempts - 1;
        if (!result.passed){
            report.failed_checkpoint = checkpoint.id;
            report.failed_event = result.run.failed_name;
            report.failure = result.run.failure;
            m_backend.log(std::format("[Segment] {}: failed at {}: {}", id, checkpoint.id, to_string(report.failure)));
            return report;
        }
        start_saved = result.saved;
    }
    report.completed = true;
    m_backend.log(std::format("[Segment] {}: done, {} checkpoints", id, report.checkpoints_run));
    return report;
}

template <StoryPose PoseType>
CheckpointReport StoryEngine<PoseType>::run_steps(const StoryCheckpoint& checkpoint, RunState& state){
    Checkpoint events{.id = checkpoint.id, .events = {}};
    for (const StoryStep& step : checkpoint.steps){
        events.events.push_back(Event{
            .name = step.name,
            .why = step.why,
            .act = [this, &step](RunState& run_state){
                return m_runner.act(step, run_state);
            },
        });
    }
    EventLogger log = [this](const std::string& line){
        m_backend.log(line);
    };
    return Pokemon::run_checkpoint(state, events, log);
}

template <StoryPose PoseType>
void StoryEngine<PoseType>::save_if_passed(const StoryCheckpoint& checkpoint, StoryEngineReport& report){
    if (!m_game.save){
        m_backend.log(std::format("[Save] {}: not saved: the game has no save", checkpoint.id));
        return;
    }
    const Outcome saved = m_game.save(m_backend, checkpoint);
    report.saved = saved.succeeded();
    m_backend.log(std::format("[Save] {}: {}", checkpoint.id, report.saved ? "saved" : std::format("save failed: {}", to_string(saved))));
}


template class StoryEngine<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
