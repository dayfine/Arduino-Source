/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <exception>
#include <format>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
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

//  Runs `step`, step `number` (from 1) of checkpoint `checkpoint_id`; a
//  throw is logged and passes through.
std::optional<StoryFailure> run_step(
    std::string_view checkpoint_id, size_t number, StoryBackend& backend, StoryStep& step, RunState& state
){
    backend.log(event_log_line(checkpoint_id, number, step.name, "start"));
    std::optional<StoryFailure> failure;
    try{
        backend.scope().throw_if_cancelled();
        failure = step.run(backend, state);
    }catch (...){
        const std::string what = current_exception_text();
        backend.log(event_log_line(checkpoint_id, number, step.name, std::format("failed: {}", what)));
        backend.log(checkpoint_log_line(checkpoint_id, std::format("failed at {} {}: {}", number, step.name, what)));
        throw;
    }
    backend.log(event_log_line(
        checkpoint_id, number, step.name, failure ? std::format("failed: {}", *failure) : "done"
    ));
    return failure;
}

//  One attempt: the steps in order until one fails, then the save when
//  they passed.
std::optional<CheckpointFailure> run_once(
    StoryBackend& backend, StoryCheckpoint& checkpoint, StoryGame& game, RunState& state
){
    const std::string& id = checkpoint.id;
    backend.log(checkpoint_start_line(id, checkpoint.steps.size()));
    for (size_t c = 0; c < checkpoint.steps.size(); c++){
        std::optional<StoryFailure> failure = run_step(id, c + 1, backend, checkpoint.steps[c], state);
        if (failure){
            backend.log(checkpoint_log_line(id, std::format("failed at {} {}: {}", c + 1, checkpoint.steps[c].name, *failure)));
            return CheckpointFailure{.checkpoint_id = id, .event_index = c, .failure = std::move(*failure)};
        }
    }
    backend.log(checkpoint_log_line(id, std::format("done, {} run", checkpoint.steps.size())));
    if (game.save){
        if (const std::optional<StoryFailure> failure = game.save(backend, checkpoint)){
            backend.log(std::format("[Save] {}: save failed: {}", id, *failure));
        }
    }
    return std::nullopt;
}

//  Empties `state` and reloads after `checkpoints[failed]` failed; returns
//  the index of the checkpoint up to `failed` whose start the game is at,
//  none when the game is at no such start.
std::optional<size_t> reload(
    StoryBackend& backend, std::span<const StoryCheckpoint> checkpoints, size_t failed, StoryGame& game, RunState& state
){
    state = RunState{};
    const std::optional<std::string> start = game.reload(backend, checkpoints[failed]);
    for (size_t c = 0; c <= failed; c++){
        if (start == checkpoints[c].id){
            return c;
        }
    }
    return std::nullopt;
}


} // namespace


std::optional<CheckpointFailure> run_checkpoints(
    StoryBackend& backend, std::span<StoryCheckpoint> checkpoints,
    StoryGame& game, RunState& state
){
    //  Failed runs of each checkpoint so far.
    std::vector<size_t> failures(checkpoints.size(), 0);
    size_t index = 0;
    while (index < checkpoints.size()){
        StoryCheckpoint& checkpoint = checkpoints[index];
        std::optional<CheckpointFailure> failure = run_once(backend, checkpoint, game, state);
        if (!failure){
            backend.log(checkpoint_pass_line(checkpoint.id, failures[index] + 1));
            index++;
            continue;
        }
        const size_t failed = ++failures[index];
        if (failure->failure.repeats || !game.reload || failed >= game.attempts_per_checkpoint){
            return failure;
        }
        backend.log(checkpoint_reset_line(checkpoint.id, failed + 1, game.attempts_per_checkpoint));
        const std::optional<size_t> start = reload(backend, checkpoints, index, game, state);
        if (!start){
            backend.log(checkpoint_log_line(checkpoint.id, "reloaded to no start of this run"));
            return CheckpointFailure{
                .checkpoint_id = checkpoint.id,
                .failure = {.code = "start_unknown", .detail = "the reload is at no start of this run"},
            };
        }
        index = *start;
    }
    return std::nullopt;
}



} // namespace Pokemon
} // namespace PokemonAutomation
