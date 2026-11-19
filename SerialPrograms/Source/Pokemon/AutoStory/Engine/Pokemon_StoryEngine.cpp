/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <exception>
#include <format>
#include <string_view>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_CheckpointAttempts.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon_StoryActionRegistry.h"
#include "Pokemon_StorySavePolicy.h"
#include "Pokemon_StoryEngine.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  "[Event] 00.04/2 to_lab: start": step `number` (from 1) of `checkpoint`,
//  named `event`, and its status.
std::string event_log_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view status){
    return std::format("[Event] {}/{} {}: {}", checkpoint, number, event, status);
}

//  "[Checkpoint] 00.04: done, 3 run".
std::string checkpoint_log_line(std::string_view checkpoint, std::string_view text){
    return std::format("[Checkpoint] {}: {}", checkpoint, text);
}

//  The text of the exception being handled.
std::string current_exception_text(){
    try{
        throw;
    }catch (const Exception& e){
        return e.message();
    }catch (std::exception& e){
        return e.what();
    }catch (...){
        return "unknown exception";
    }
}

//  A failure outside the steps: a lint or a reload.
CheckpointRunResult failed_outside_steps(Outcome outcome){
    return CheckpointRunResult{.failure = CheckpointFailure{.event_index = std::nullopt, .outcome = std::move(outcome)}};
}

//  Whether the run is on a save made at `checkpoint`'s end.
bool save_checkpoint(const StoryGame& game, StoryBackend& backend, const StoryCheckpoint& checkpoint){
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

//  Runs step `number` (from 1) of `checkpoint`; a throw is logged and
//  passes through.
Outcome run_step(
    const StoryGame& game, StoryBackend& backend, const StoryCheckpoint& checkpoint, size_t number, RunState& state
){
    const StoryStep& step = checkpoint.steps[number - 1];
    backend.log(event_log_line(checkpoint.id, number, step.name, "start"));
    Outcome outcome;
    try{
        outcome = run_story_step(game, backend, step, state);
    }catch (...){
        const std::string what = current_exception_text();
        backend.log(event_log_line(checkpoint.id, number, step.name, std::format("failed: {}", what)));
        backend.log(checkpoint_log_line(checkpoint.id, std::format("failed at {} {}: {}", number, step.name, what)));
        throw;
    }
    backend.log(event_log_line(
        checkpoint.id, number, step.name, outcome.succeeded() ? "done" : std::format("failed: {}", to_string(outcome))
    ));
    return outcome;
}

//  One attempt: lint, the steps in order until one fails, then the save
//  when they passed.
CheckpointRunResult run_once(
    const StoryGame& game, StoryBackend& backend, const StoryCheckpoint& checkpoint, RunState& state
){
    const std::vector<std::string> problems = lint_story_checkpoint(game, checkpoint);
    for (const std::string& problem : problems){
        backend.log(std::format("[Lint] {}", problem));
    }
    if (!problems.empty()){
        return failed_outside_steps(outcome_failed("lint_failed", problems.front()));
    }
    const std::string& id = checkpoint.id;
    backend.log(checkpoint_start_line(id, checkpoint.steps.size()));
    for (size_t c = 0; c < checkpoint.steps.size(); c++){
        Outcome outcome = run_step(game, backend, checkpoint, c + 1, state);
        if (!outcome.succeeded()){
            backend.log(checkpoint_log_line(id, std::format("failed at {} {}: {}", c + 1, checkpoint.steps[c].name, to_string(outcome))));
            backend.log(std::format("[Save] {}: run failed: not saved", id));
            return CheckpointRunResult{.failure = CheckpointFailure{.event_index = c, .outcome = std::move(outcome)}};
        }
    }
    backend.log(checkpoint_log_line(id, std::format("done, {} run", checkpoint.steps.size())));
    return CheckpointRunResult{.failure = std::nullopt, .saved = save_checkpoint(game, backend, checkpoint)};
}

//  Empties `state` and reloads the last save.
Outcome reload(const StoryGame& game, StoryBackend& backend, RunState& state){
    state = RunState{};
    if (!game.reload){
        return outcome_failed("reload_failed", "the game has no reload");
    }
    const Outcome reloaded = game.reload(backend);
    if (!reloaded.succeeded()){
        return outcome_failed("reload_failed", to_string(reloaded));
    }
    return outcome_ok();
}

//  `checkpoint` until it passes or `attempts` runs failed.
CheckpointRun run_with_retries(
    const StoryGame& game, StoryBackend& backend,
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
            const bool repeats = failure == GAME_NOT_RUNNING || failure == "lint_failed";
            return repeats ? AttemptResult::FINAL : AttemptResult::FAILED;
        },
        .reset = [&](size_t next_attempt){
            backend.log(checkpoint_reset_line(checkpoint.id, next_attempt, attempts));
            Outcome reloaded = reload(game, backend, state);
            if (!reloaded.succeeded()){
                backend.log(checkpoint_log_line(checkpoint.id, to_string(reloaded)));
                run.result = failed_outside_steps(std::move(reloaded));
                return false;
            }
            return true;
        },
        .log = [&backend](std::string_view line){
            backend.log(line);
        },
    };
    run_checkpoint_attempts(checkpoint.id, loop);
    return run;
}


} // namespace


std::vector<CheckpointRun> run_checkpoints(
    const StoryGame& game, StoryBackend& backend,
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
        runs.push_back(run_with_retries(game, backend, checkpoint, attempts, state));
        if (runs.back().result.failure){
            break;
        }
        start_saved = runs.back().result.saved;
    }
    return runs;
}



} // namespace Pokemon
} // namespace PokemonAutomation
