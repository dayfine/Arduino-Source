/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The one story executor, for every game and both backends. A run of a
 *  checkpoint lints its steps against the game's actions (a problem fails
 *  the run before any input), runs them through the event layer
 *  (Pokemon_StoryEvents.h: skip_if, retries, recoveries, interrupts) with
 *  the step runner, checks the postconditions with the game's readers, and
 *  saves only as the checkpoint's phase allows (Pokemon_StoryEnginePolicy.h).
 *
 *  See MIGRATION.md in this folder for moving a game onto it.
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
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StorySaveMode.h"
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

    //  story_steps_hash() of `checkpoint` with this game's actions.
    std::string steps_hash(const StoryCheckpoint& checkpoint) const;

    //  Runs `checkpoint` from `state`, then checks its postconditions and
    //  saves if its phase allows a save at `point`. A SAVE step among its
    //  steps saves only in PER_CHECKPOINT mode.
    StoryEngineReport run_checkpoint(const StoryCheckpoint& checkpoint, SavePoint point, RunState& state);

    //  `checkpoints` in order from the save the game is on, each run with
    //  run_checkpoint() (the last at RUN_END), until one does not pass.
    //  One that fails is run again after a reload (StoryGame::reload), up to
    //  `attempts_per_checkpoint` runs, only when a reload returns to the
    //  save it started from: the first checkpoint's, or one the checkpoint
    //  before it saved at its end. In none and after_verification modes
    //  (#329) that leaves only the first. No reload either for a failure
    //  every attempt would repeat: a lint, a console_only action, the game
    //  not running. Logs "[Segment] <id>: ..." lines.
    SegmentReport run_segment(
        std::string_view id, std::span<const StoryCheckpoint> checkpoints, size_t attempts_per_checkpoint,
        RunState& state
    );

    //  Verify mode: `checkpoint` run `policy.runs` times, each from the save
    //  the first started from (a reload before each run after the first).
    //  No run saves, but the last when every run before it passed: it saves
    //  at RUN_END as the checkpoint's phase allows. `policy.save_mode` is
    //  not read; the phase decides. Logs "[Verify] <id>: ..." lines.
    VerifyReport verify_checkpoint(const StoryCheckpoint& checkpoint, const VerifyPolicy& policy, RunState& state);

    //  `checkpoint`'s postconditions against a fresh observation, read by
    //  the game's readers that opted in (StoryReaderUse::AFTER_CHECKPOINT)
    //  and supply what they need, but no console_only one off a console;
    //  logged as "[Postcondition]" lines.
    PostconditionReport check_postconditions(const StoryCheckpoint& checkpoint);

private:
    //  Whether a run may save at all.
    enum class SaveGate{
        BY_PHASE,
        NEVER,
    };

    StoryEngineReport run_once(const StoryCheckpoint& checkpoint, SavePoint point, SaveGate gate, RunState& state);
    CheckpointReport run_steps(const StoryCheckpoint& checkpoint, RunState& state);
    void save_if_allowed(const StoryCheckpoint& checkpoint, SavePoint point, StoryEngineReport& report);
    //  Empties `state` and reloads the last save; the reload's pose, if it
    //  says one, goes into `state`.
    Outcome reload(RunState& state);
    //  One checkpoint of a segment until it passes or runs out of attempts.
    StoryEngineReport run_with_retries(
        const StoryCheckpoint& checkpoint, SavePoint point, size_t attempts, RunState& state, CheckpointRun& run
    );

    //  Declared first: m_game's save step reads them.
    StorySaveMode m_save_mode = StorySaveMode::NONE;
    const StoryCheckpoint* m_checkpoint = nullptr;
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
