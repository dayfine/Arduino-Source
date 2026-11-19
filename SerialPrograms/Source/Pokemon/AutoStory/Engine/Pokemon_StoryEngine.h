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
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryBackend.h"
#include "Pokemon_StoryGame.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Where and why a checkpoint run stopped.
struct CheckpointFailure{
    //  0-based; none when the checkpoint failed outside its steps (a lint
    //  or a reload).
    std::optional<size_t> event_index = std::nullopt;
    Outcome outcome = {};
};

//  How a checkpoint run ended.
struct CheckpointRunResult{
    //  None: every step passed.
    std::optional<CheckpointFailure> failure = std::nullopt;
    //  The run is on a save made at the checkpoint's end.
    bool saved = false;
};

//  One checkpoint of a run_checkpoints() run: its last attempt's result.
struct CheckpointRun{
    std::string id = {};
    //  Counting the last.
    size_t attempts = 0;
    CheckpointRunResult result = {};
};

//  Runs `checkpoints` in order from `state` until one fails; returns each
//  one's run, the failed one last. A failed checkpoint is retried after
//  game.reload only when a reload returns to its start. A step that throws
//  is logged and the exception passes through.
std::vector<CheckpointRun> run_checkpoints(
    const StoryGame& game, StoryBackend& backend,
    std::span<const StoryCheckpoint> checkpoints, RunState& state
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
