/*  Story Action Registry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include <string_view>
#include "Pokemon_StoryActionRegistry.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The action of `kind` named `name`; for an empty name, the kind's first.
const StoryAction* find_action(const StoryGame& game, StoryStepKind kind, std::string_view name){
    for (const StoryAction& action : game.actions){
        if (action.spec.kind == kind && (name.empty() || action.spec.name == name)){
            return &action;
        }
    }
    return nullptr;
}


} // namespace


std::vector<std::string> lint_story_checkpoint(const StoryGame& game, const StoryCheckpoint& checkpoint){
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
        if (step.kind == StoryStepKind::GAME && step.action.empty()){
            problems.push_back(std::format("{}: no action", where));
            continue;
        }
        const StoryAction* action = find_action(game, step.kind, step.action);
        if (action == nullptr){
            problems.push_back(std::format("{}: unknown action \"{}\" for {}", where, step.action, story_step_kind_name(step.kind)));
            continue;
        }
        for (const std::string& problem : story_args_errors(action->spec, step.args)){
            problems.push_back(std::format("{}: {}", where, problem));
        }
    }
    return problems;
}

Outcome run_story_step(const StoryGame& game, StoryBackend& backend, const StoryStep& step, RunState& state){
    backend.scope().throw_if_cancelled();
    const std::string_view action_name = step.action.empty() ? std::string_view("default") : std::string_view(step.action);
    const StoryAction* action = find_action(game, step.kind, step.action);
    Outcome outcome;
    if (action == nullptr || !action->handler){
        outcome = outcome_failed("unsupported_step", std::format(
            "{} {}: the game has no action for it", story_step_kind_name(step.kind), action_name
        ));
    }else{
        const StoryActionArgs args = with_fallbacks(action->spec, step.args);
        outcome = action->handler(backend, StoryActionCall{.step = step, .args = args}, state);
    }
    backend.log(std::format(
        "[Step] {} {}/{}: {}", step.name, story_step_kind_name(step.kind), action_name,
        outcome.succeeded() ? "ok" : std::format("failed: {}", to_string(outcome))
    ));
    return outcome;
}



} // namespace Pokemon
} // namespace PokemonAutomation
