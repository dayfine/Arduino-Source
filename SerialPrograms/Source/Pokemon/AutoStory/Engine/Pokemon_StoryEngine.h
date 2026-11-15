/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The one story executor, for every game and both backends. A run of a
 *  checkpoint lints its steps against the game's actions (a problem fails
 *  the run before any input), runs them through the event layer
 *  (Pokemon_StoryEvents.h: skip_if, retries, recoveries, interrupts) with
 *  the step runner, checks the postconditions with the game's readers, and
 *  saves at the end of each checkpoint that passed (StoryGame::save: the
 *  game knows whether the row autosaved or needs its menu's save).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEngine_H
#define PokemonAutomation_Pokemon_StoryEngine_H

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StorySegmentTypes.h"
#include "Pokemon_StoryActionRegistry.h"
#include "Pokemon_StoryEnginePolicy.h"
#include "Pokemon_StoryStepRunner.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a look at a tile game shows of a checkpoint's postconditions: the
//  map, the tile (when the pose is one tile) and the facing from the pose;
//  items from "bag.<ITEM>" readings; flags from "flag.<name>" readings
//  (non-zero is set).
ObservedState observed_state(const Observation<GridPose>& observation);

//  The same for a zone game: the map is the zone; no tile or facing.
ObservedState observed_state(const Observation<ZonePose>& observation);

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

    //  Runs `checkpoint` from `state`, then checks its postconditions and,
    //  when it passed, saves (StoryGame::save). A SAVE step among its steps
    //  saves too.
    StoryEngineReport run_checkpoint(const StoryCheckpoint& checkpoint, RunState& state);

    //  `checkpoints` in order from the save the game is on, each run with
    //  run_checkpoint(), until one does not pass.
    //  One that fails is run again after a reload (StoryGame::reload), up to
    //  `attempts_per_checkpoint` runs, only when a reload returns to the
    //  save it started from: the first checkpoint's, or one the checkpoint
    //  before it saved at its end. No reload once a SAVE step has saved
    //  part-way through the attempt (the reload would land there), nor for
    //  a failure every attempt would repeat: a lint, a console_only action,
    //  the game not running. Logs "[Segment] <id>: ..." lines.
    SegmentReport run_segment(
        std::string_view id, std::span<const StoryCheckpoint> checkpoints, size_t attempts_per_checkpoint,
        RunState& state
    );

    //  `checkpoint`'s postconditions against a fresh observation, read by
    //  the game's readers that opted in (StoryReaderUse::AFTER_CHECKPOINT)
    //  and supply what they need, but no console_only one off a console;
    //  logged as "[Postcondition]" lines.
    PostconditionReport check_postconditions(const StoryCheckpoint& checkpoint);

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

    //  Declared first: m_game's save step reads them, and sets
    //  m_saved_part_way when it saves.
    const StoryCheckpoint* m_checkpoint = nullptr;
    bool m_saved_part_way = false;
    StoryGame<PoseType> m_game;
    StoryBackend<PoseType>& m_backend;
    StoryActionRegistry<PoseType> m_registry;
    StoryStepRunner<PoseType> m_runner;
    PreparedSteps m_prepared;
};

//  Instantiated once, in Pokemon_StoryEngine.cpp.
extern template class StoryEngine<GridPose>;
//  And for ZonePose (Pokemon_ZonePose.h).
extern template class StoryEngine<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
