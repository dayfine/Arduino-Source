/*  Story Step Runner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs one story step on a backend: runs its action through the registry
 *  and logs a "[Step]" line. One step executor for every game that has a
 *  StoryBackend, on a console or in a simulation.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStepRunner_H
#define PokemonAutomation_Pokemon_StoryStepRunner_H

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

    //  `step`'s action, once the run is not cancelled, then the "[Step]"
    //  line.
    Outcome act(const StoryStep& step, RunState& state);

private:
    const StoryActionRegistry<PoseType>& m_registry;
    StoryBackend<PoseType>& m_backend;
};

//  Instantiated for ZonePose (Pokemon_ZonePose.h), in
//  Pokemon_StoryStepRunner.cpp.
extern template class StoryStepRunner<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
