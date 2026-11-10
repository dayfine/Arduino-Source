/*  Story Step Runner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs one story step on a backend: skips it when its skip_if conditions
 *  hold, runs its action through the registry, writes its declared end
 *  pose into the outcome, and logs a "[Step]" line. The one step executor
 *  of every game, on a console or in a simulation; it replaces
 *  StoryStepExecutor and the games' own executors.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStepRunner_H
#define PokemonAutomation_Pokemon_StoryStepRunner_H

#include <string_view>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon_StoryActionRegistry.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Runs steps with `registry`'s actions on `backend`. Both must outlive it.
template <StoryPose PoseType>
class StoryStepRunner{
public:
    StoryStepRunner(const StoryActionRegistry<PoseType>& registry, StoryBackend<PoseType>& backend);
    StoryStepRunner(const StoryStepRunner&) = delete;
    StoryStepRunner& operator=(const StoryStepRunner&) = delete;

    //  `step`, unless all its skip_if conditions hold (and it has some):
    //  then it changes nothing and says so.
    Outcome run(RunState& state, const StoryStep& step);

    //  `step`'s action without the skip_if check, for a caller that checks
    //  it itself (the event layer): dispatch, then the declared end pose
    //  when the action gave none, then the "[Step]" line.
    Outcome act(RunState& state, const StoryStep& step);

    //  Whether `condition` holds: a flag of the run, or else a non-zero
    //  reading from the game's reader of that name on a fresh observation.
    bool holds(std::string_view condition, RunState& state);

private:
    const StoryActionRegistry<PoseType>& m_registry;
    StoryBackend<PoseType>& m_backend;
};

//  Instantiated once, in Pokemon_StoryStepRunner.cpp.
extern template class StoryStepRunner<GridPose>;
//  And for ZonePose (Pokemon_ZonePose.h).
extern template class StoryStepRunner<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
