/*  Story Step Runner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon_StoryStepRunner.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  "route/route" or "game_action/take_warp": the step's kind and action.
inline std::string step_label(const StoryStep& step){
    return std::format("{}/{}", story_step_kind_name(step.kind), step.action.empty() ? "default" : step.action);
}


} // namespace


template <StoryPose PoseType>
StoryStepRunner<PoseType>::StoryStepRunner(const StoryActionRegistry<PoseType>& registry, StoryBackend<PoseType>& backend)
    : m_registry(registry)
    , m_backend(backend)
{}

template <StoryPose PoseType>
Outcome StoryStepRunner<PoseType>::run(const StoryStep& step, RunState& state){
    bool skip = !step.skip_if.empty();
    for (const std::string& condition : step.skip_if){
        skip = skip && holds(condition, state);
    }
    if (skip){
        m_backend.log(std::format("[Step] {} {}: skipped", step.name, step_label(step)));
        return outcome_unchanged();
    }
    return act(step, state);
}

template <StoryPose PoseType>
Outcome StoryStepRunner<PoseType>::act(const StoryStep& step, RunState& state){
    m_backend.scope().throw_if_cancelled();
    Outcome outcome = m_registry.dispatch(m_backend, step, state);
    if (outcome.succeeded() && !outcome.pose && step.pose_after){
        outcome.pose = pose_of(*step.pose_after);
    }
    m_backend.log(std::format(
        "[Step] {} {}: {}", step.name, step_label(step),
        outcome.succeeded() ? "ok" : std::format("failed: {}", to_string(outcome))
    ));
    return outcome;
}

template <StoryPose PoseType>
bool StoryStepRunner<PoseType>::holds(std::string_view condition, RunState& state){
    if (state.flags.contains(std::string(condition))){
        return true;
    }
    for (const StoryGameReader<PoseType>& reader : m_registry.game().readers){
        if (reader.name != condition || !reader.read){
            continue;
        }
        if (reader.console_only && m_backend.kind() != StoryBackendKind::CONSOLE){
            continue;
        }
        Observation<PoseType> observation = m_backend.observe();
        reader.read(m_backend, observation);
        const auto reading = observation.readings.find(condition);
        return reading != observation.readings.end() && reading->second != 0;
    }
    return false;
}


template class StoryStepRunner<GridPose>;
template class StoryStepRunner<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
