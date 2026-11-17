/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The one story executor, for every game and both backends. A run of a
 *  checkpoint lints its steps against the game's actions (a problem fails
 *  the run before any input), runs them through the event layer
 *  (Pokemon_StoryEvents.h) with the step runner, and saves at the end of
 *  each checkpoint that passed (StoryGame::save: the game knows whether
 *  the row autosaved). A failed checkpoint is retried after the game's
 *  reload, when the reload returns to the save it started from.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEngine_H
#define PokemonAutomation_Pokemon_StoryEngine_H

#include <cstddef>
#include <span>
#include <string_view>
#include "Pokemon/AutoStory/Pokemon_StoryCheckpointTypes.h"
#include "Pokemon/AutoStory/Types/Pokemon_StorySegmentTypes.h"
#include "Pokemon_StoryActionRegistry.h"
#include "Pokemon_StoryStepRunner.h"

namespace PokemonAutomation{
namespace Pokemon{


//  An engine run of one checkpoint.
struct StoryEngineReport{
    //  The steps' run. A checkpoint that failed its lint has none.
    CheckpointReport run = {};
    //  The steps all completed.
    bool passed = false;
    //  The game saved at the checkpoint's end.
    bool saved = false;
};

//  Runs `game`'s checkpoints on `backend`. Both must outlive it.
template <StoryPose PoseType>
class StoryEngine{
public:
    StoryEngine(const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend);
    StoryEngine(const StoryEngine&) = delete;
    StoryEngine& operator=(const StoryEngine&) = delete;

    const StoryActionRegistry<PoseType>& registry() const{
        return m_registry;
    }

    //  Runs `checkpoint` from `state` and, when it passed, saves
    //  (StoryGame::save).
    StoryEngineReport run_checkpoint(const StoryCheckpoint& checkpoint, RunState& state);

    //  `checkpoints` in order from the save the game is on, each run with
    //  run_checkpoint(), until one does not pass. One that fails is run
    //  again after a reload (StoryGame::reload), up to
    //  `attempts_per_checkpoint` runs, only when a reload returns to the
    //  save it started from: the first checkpoint's, or one the checkpoint
    //  before it saved at its end. No reload for a failure every attempt
    //  would repeat: a lint, a console_only action, the game not running.
    //  Logs "[Segment] <id>: ..." lines.
    SegmentReport run_segment(
        std::string_view id, std::span<const StoryCheckpoint> checkpoints, size_t attempts_per_checkpoint,
        RunState& state
    );

private:
    CheckpointReport run_steps(const StoryCheckpoint& checkpoint, RunState& state);
    void save_if_passed(const StoryCheckpoint& checkpoint, StoryEngineReport& report);
    //  Empties `state` and reloads the last save; the reload's pose, if it
    //  says one, goes into `state`.
    Outcome reload(RunState& state);
    //  One checkpoint of a segment until it passes or runs out of attempts.
    StoryEngineReport run_with_retries(
        const StoryCheckpoint& checkpoint, size_t attempts, RunState& state, CheckpointRun& run
    );

    const StoryGame<PoseType>& m_game;
    StoryBackend<PoseType>& m_backend;
    StoryActionRegistry<PoseType> m_registry;
    StoryStepRunner<PoseType> m_runner;
};

//  Instantiated for ZonePose (Pokemon_ZonePose.h), in
//  Pokemon_StoryEngine.cpp.
extern template class StoryEngine<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
