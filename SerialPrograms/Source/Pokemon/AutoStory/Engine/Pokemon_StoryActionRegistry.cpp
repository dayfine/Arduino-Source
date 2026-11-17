/*  Story Action Registry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include "Pokemon_StoryActionRegistry.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  The failure of a step whose action the game does not have.
constexpr std::string_view UNSUPPORTED_STEP = "unsupported_step";

} // namespace


template <StoryPose PoseType>
StoryActionRegistry<PoseType>::StoryActionRegistry(const StoryGame<PoseType>& game)
    : m_game(game)
{
    for (const StoryAction<PoseType>& action : game.actions){
        m_specs.push_back(action.spec);
    }
}

template <StoryPose PoseType>
const StoryAction<PoseType>* StoryActionRegistry<PoseType>::find(StoryStepKind kind, std::string_view name) const{
    for (const StoryAction<PoseType>& action : m_game.actions){
        if (action.spec.kind == kind && (name.empty() || action.spec.name == name)){
            return &action;
        }
    }
    return nullptr;
}

template <StoryPose PoseType>
std::vector<std::string> StoryActionRegistry<PoseType>::check(const StoryStep& step) const{
    if (step.kind == StoryStepKind::GAME && step.action.empty()){
        return {"no action"};
    }
    const StoryAction<PoseType>* action = find(step.kind, step.action);
    if (action == nullptr){
        return {std::format("unknown action \"{}\" for {}", step.action, story_step_kind_name(step.kind))};
    }
    return story_args_errors(action->spec, step.args);
}

template <StoryPose PoseType>
std::vector<std::string> StoryActionRegistry<PoseType>::lint(const StoryCheckpoint& checkpoint) const{
    std::vector<std::string> problems;
    if (checkpoint.id.empty()){
        problems.emplace_back("a checkpoint has no id");
    }
    std::set<std::string> names;
    for (size_t c = 0; c < checkpoint.steps.size(); c++){
        const StoryStep& step = checkpoint.steps[c];
        const std::string where = std::format("{} step {} {}", checkpoint.id, c + 1, step.name);
        if (step.name.empty() || !names.insert(step.name).second){
            problems.push_back(std::format("{}: empty or repeated name", where));
        }
        for (const std::string& problem : check(step)){
            problems.push_back(std::format("{}: {}", where, problem));
        }
    }
    return problems;
}

template <StoryPose PoseType>
Outcome StoryActionRegistry<PoseType>::dispatch(StoryBackend<PoseType>& backend, const StoryStep& step, RunState& state) const{
    const StoryAction<PoseType>* action = find(step.kind, step.action);
    if (action == nullptr || !action->handler){
        return outcome_failed(std::string(UNSUPPORTED_STEP), std::format(
            "{} {}: the game has no action for it", story_step_kind_name(step.kind), step.action.empty() ? "default" : step.action
        ));
    }
    if (action->spec.console_only && backend.kind() != StoryBackendKind::CONSOLE){
        return outcome_failed(std::string(CONSOLE_ONLY), std::format(
            "{} runs only on a console, not on a {} backend", action->spec.name, story_backend_kind_name(backend.kind())
        ));
    }
    const StoryActionArgs args = with_fallbacks(action->spec, step.args);
    return action->handler(backend, StoryActionCall{.step = step, .args = args}, state);
}


template class StoryActionRegistry<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
