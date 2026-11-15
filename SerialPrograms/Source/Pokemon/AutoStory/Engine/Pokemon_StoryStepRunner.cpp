/*  Story Step Runner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Pokemon_StoryStepRunner.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  "GAME/walk" or "TALK/default": the step's kind and action.
std::string step_label(const StoryStep& step){
    return std::format("{}/{}", story_step_kind_name(step.kind), step.action.empty() ? "default" : step.action);
}

} // namespace


template <StoryPose PoseType>
StoryStepRunner<PoseType>::StoryStepRunner(const StoryActionRegistry<PoseType>& registry, StoryBackend<PoseType>& backend)
    : m_registry(registry)
    , m_backend(backend)
{}

template <StoryPose PoseType>
Outcome StoryStepRunner<PoseType>::act(const StoryStep& step, RunState& state){
    m_backend.scope().throw_if_cancelled();
    Outcome outcome = m_registry.dispatch(m_backend, step, state);
    m_backend.log(std::format(
        "[Step] {} {}: {}", step.name, step_label(step),
        outcome.succeeded() ? "ok" : std::format("failed: {}", to_string(outcome))
    ));
    return outcome;
}


template class StoryStepRunner<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
