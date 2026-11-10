/*  Story Action Registry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <set>
#include <utility>
#include "Pokemon/AutoStory/Pokemon_MapUtil.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon_StoryActionRegistry.h"
#include "Pokemon_StoryBuiltinActions.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  A walk of no legs, or a leg of no tiles.
inline std::vector<std::string> leg_problems(const StoryActionArgs& args){
    const std::vector<StoryLeg>* legs = value_if_at_it<std::vector<StoryLeg>>(args, args.find("legs"));
    if (legs == nullptr){
        return {};
    }
    std::vector<std::string> problems;
    if (legs->empty()){
        problems.emplace_back("no legs");
    }
    for (const StoryLeg& leg : *legs){
        if (leg.tiles <= 0){
            problems.emplace_back("a leg of no tiles");
        }
    }
    return problems;
}


} // namespace


template <StoryPose PoseType>
StoryActionRegistry<PoseType>::StoryActionRegistry(const StoryGame<PoseType>& game)
    : m_game(game)
    , m_actions(builtin_story_handlers(game))
{
    //  A game's action replaces the built-in of its kind and name in place,
    //  so a kind's default stays the built-in one's position.
    for (const StoryAction<PoseType>& action : game.actions){
        const auto same = std::find_if(m_actions.begin(), m_actions.end(), [&](const StoryAction<PoseType>& builtin){
            return builtin.spec.kind == action.spec.kind && builtin.spec.name == action.spec.name;
        });
        if (same != m_actions.end()){
            *same = action;
        }else{
            m_actions.push_back(action);
        }
    }
    for (const StoryAction<PoseType>& action : m_actions){
        m_specs.push_back(action.spec);
    }
}

template <StoryPose PoseType>
const StoryAction<PoseType>* StoryActionRegistry<PoseType>::find(StoryStepKind kind, std::string_view name) const{
    for (const StoryAction<PoseType>& action : m_actions){
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
    std::vector<std::string> problems = story_args_errors(action->spec, step.args);
    for (std::string& problem : leg_problems(step.args)){
        problems.push_back(std::move(problem));
    }
    return problems;
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
            problems.push_back(where + ": empty or repeated name");
        }
        for (const std::string& problem : check(step)){
            problems.push_back(std::format("{}: {}", where, problem));
        }
    }
    return problems;
}

template <StoryPose PoseType>
Outcome StoryActionRegistry<PoseType>::dispatch(StoryBackend<PoseType>& backend, RunState& state, const StoryStep& step) const{
    const StoryAction<PoseType>* action = find(step.kind, step.action);
    if (action == nullptr || !action->handler){
        return unsupported_step(step);
    }
    return action->handler(backend, state, with_fallbacks(action->spec, step.args));
}


template class StoryActionRegistry<GridPose>;
template class StoryActionRegistry<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
