/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs a range of story checkpoints on a game's backend.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEngine_H
#define PokemonAutomation_Pokemon_StoryEngine_H

#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_CheckpointAttempts.h"
#include "Pokemon/AutoStory/Pokemon_StoryCheckpointTypes.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon_StoryActionRegistry.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One checkpoint of a run_checkpoints() run: its last attempt's result.
struct CheckpointRun{
    std::string id = {};
    //  Counting the last.
    size_t attempts = 0;
    CheckpointRunResult result = {};
};

//  Runs `checkpoints` in order from `state` until one fails; returns each
//  one's run, the failed one last. A failed checkpoint is retried after
//  game.reload only when a reload returns to its start.
template <StoryPose PoseType>
std::vector<CheckpointRun> run_checkpoints(
    const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend,
    std::span<const StoryCheckpoint> checkpoints, RunState& state
);


namespace StoryEngineInternal{

//  A failure outside the events: a lint or a reload.
inline CheckpointRunResult failed_outside_events(Outcome outcome){
    return CheckpointRunResult{.failure = CheckpointFailure{.event_index = std::nullopt, .outcome = std::move(outcome)}};
}

//  Whether the run is on a save made at `checkpoint`'s end.
template <StoryPose PoseType>
bool save_checkpoint(const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend, const StoryCheckpoint& checkpoint){
    if (!game.save){
        backend.log(std::format("[Save] {}: not saved: the game has no save", checkpoint.id));
        return false;
    }
    const Outcome saved = game.save(backend, checkpoint);
    if (saved.succeeded()){
        backend.log(std::format("[Save] {}: saved", checkpoint.id));
    }else if (saved.failure == NOT_SAVED){
        backend.log(std::format("[Save] {}: not saved ({})", checkpoint.id, saved.detail));
    }else{
        backend.log(std::format("[Save] {}: save failed: {}", checkpoint.id, to_string(saved)));
    }
    return saved.succeeded();
}

//  One attempt: lint, the steps, then the save when they passed.
template <StoryPose PoseType>
CheckpointRunResult run_once(
    const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend, const StoryCheckpoint& checkpoint, RunState& state
){
    const std::vector<std::string> problems = lint_story_checkpoint(game, checkpoint);
    for (const std::string& problem : problems){
        backend.log(std::format("[Lint] {}", problem));
    }
    if (!problems.empty()){
        return failed_outside_events(outcome_failed("lint_failed", problems.front()));
    }
    Checkpoint events{.id = checkpoint.id, .events = {}};
    for (const StoryStep& step : checkpoint.steps){
        events.events.push_back(Event{
            .name = step.name,
            .why = step.why,
            .act = [&game, &backend, &step](RunState& run_state){
                return run_story_step(game, backend, step, run_state);
            },
        });
    }
    EventLogger log = [&backend](std::string_view line){
        backend.log(line);
    };
    CheckpointRunResult result = run_checkpoint(state, events, log);
    if (result.failure){
        backend.log(std::format("[Save] {}: run failed: not saved", checkpoint.id));
    }else{
        result.saved = save_checkpoint(game, backend, checkpoint);
    }
    return result;
}

//  Empties `state` and reloads the last save; its pose, if it says one,
//  goes into `state`.
template <StoryPose PoseType>
Outcome reload(const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend, RunState& state){
    state = RunState{};
    if (!game.reload){
        return outcome_failed("reload_failed", "the game has no reload");
    }
    const Outcome reloaded = game.reload(backend);
    if (!reloaded.succeeded()){
        return outcome_failed("reload_failed", to_string(reloaded));
    }
    if (reloaded.pose){
        state.pose = *reloaded.pose;
    }
    return outcome_ok();
}

//  `checkpoint` until it passes or `attempts` runs failed.
template <StoryPose PoseType>
CheckpointRun run_with_retries(
    const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend,
    const StoryCheckpoint& checkpoint, size_t attempts, RunState& state
){
    CheckpointRun run{.id = checkpoint.id};
    const CheckpointAttempts loop{
        .max_attempts = attempts,
        .run = [&](size_t attempt){
            run.attempts = attempt;
            run.result = run_once(game, backend, checkpoint, state);
            if (!run.result.failure){
                return AttemptResult::PASSED;
            }
            //  Failures every attempt would repeat.
            const std::string& failure = run.result.failure->outcome.failure;
            const bool repeats = failure == GAME_NOT_RUNNING || failure == CONSOLE_ONLY || failure == "lint_failed";
            return repeats ? AttemptResult::FINAL : AttemptResult::FAILED;
        },
        .reset = [&](size_t next_attempt){
            backend.log(checkpoint_reset_line(checkpoint.id, next_attempt, attempts));
            Outcome reloaded = reload(game, backend, state);
            if (!reloaded.succeeded()){
                backend.log(checkpoint_log_line(checkpoint.id, to_string(reloaded)));
                run.result = failed_outside_events(std::move(reloaded));
                return false;
            }
            return true;
        },
        .log = [&backend](const std::string& line){
            backend.log(line);
        },
    };
    run_checkpoint_attempts(checkpoint.id, loop);
    return run;
}

} // namespace StoryEngineInternal


template <StoryPose PoseType>
std::vector<CheckpointRun> run_checkpoints(
    const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend,
    std::span<const StoryCheckpoint> checkpoints, RunState& state
){
    std::vector<CheckpointRun> runs;
    //  Whether a reload returns to the next checkpoint's start.
    bool start_saved = true;
    for (const StoryCheckpoint& checkpoint : checkpoints){
        if (!start_saved && game.attempts_per_checkpoint > 1){
            backend.log(checkpoint_log_line(checkpoint.id, "no save at its start, so no reload if it fails"));
        }
        const size_t attempts = start_saved ? game.attempts_per_checkpoint : 1;
        runs.push_back(StoryEngineInternal::run_with_retries(game, backend, checkpoint, attempts, state));
        if (runs.back().result.failure){
            break;
        }
        start_saved = runs.back().result.saved;
    }
    return runs;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
