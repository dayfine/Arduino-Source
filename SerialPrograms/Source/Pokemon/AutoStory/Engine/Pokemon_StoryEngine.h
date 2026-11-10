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

#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StorySaveMode.h"
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
    StoryEngineReport run_checkpoint(const StoryCheckpoint& checkpoint, RunState& state, SavePoint point = SavePoint::RUN_END);

    //  `checkpoint`'s postconditions against a fresh observation read by
    //  every reader of the game; logged as "[Postcondition]" lines.
    PostconditionReport check_postconditions(const StoryCheckpoint& checkpoint);

private:
    CheckpointReport run_steps(const StoryCheckpoint& checkpoint, RunState& state);
    void save_if_allowed(StoryEngineReport& report, const StoryCheckpoint& checkpoint, SavePoint point);

    //  Declared first: m_game's save step reads it.
    StorySaveMode m_save_mode = StorySaveMode::NONE;
    StoryGame<PoseType> m_game;
    StoryBackend<PoseType>& m_backend;
    StoryActionRegistry<PoseType> m_registry;
    StoryStepRunner<PoseType> m_runner;
    PreparedSteps m_prepared;
};

//  Instantiated once, in Pokemon_StoryEngine.cpp.
extern template class StoryEngine<GridPose>;
extern template class StoryEngine<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
