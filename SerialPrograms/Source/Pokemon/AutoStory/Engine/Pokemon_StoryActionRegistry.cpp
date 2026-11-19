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


//  The index of the action of `kind` named `name`; for an empty name, the
//  kind's first.
std::optional<size_t> find_action(const StoryGame& game, StoryStepKind kind, std::string_view name){
    for (size_t c = 0; c < game.actions.size(); c++){
        const StoryActionSpec& spec = game.actions[c].spec;
        if (spec.kind == kind && (name.empty() || spec.name == name)){
            return c;
        }
    }
    return std::nullopt;
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
        const std::optional<size_t> index = find_action(game, step.kind, step.action);
        if (!index){
            problems.push_back(std::format("{}: unknown action \"{}\" for {}", where, step.action, story_step_kind_name(step.kind)));
            continue;
        }
        for (const std::string& problem : story_args_errors(game.actions[*index].spec, step.args)){
            problems.push_back(std::format("{}: {}", where, problem));
        }
    }
    return problems;
}

std::optional<StoryFailure> run_story_step(StoryBackend& backend, const StoryStep& step, StoryGame& game, RunState& state){
    backend.scope().throw_if_cancelled();
    const std::string_view action_name = step.action.empty() ? std::string_view("default") : std::string_view(step.action);
    const std::optional<size_t> index = find_action(game, step.kind, step.action);
    std::optional<StoryFailure> failure;
    if (!index || !game.actions[*index].handler){
        failure = StoryFailure{
            .code = "unsupported_step",
            .detail = std::format("{} {}: the game has no action for it", story_step_kind_name(step.kind), action_name),
            .repeats = true,
        };
    }else{
        StoryAction& action = game.actions[*index];
        const StoryActionArgs args = with_fallbacks(action.spec, step.args);
        failure = action.handler(backend, StoryActionCall{.step = step, .args = args}, state);
    }
    backend.log(std::format(
        "[Step] {} {}/{}: {}", step.name, story_step_kind_name(step.kind), action_name,
        failure ? std::format("failed: {}", *failure) : "ok"
    ));
    return failure;
}



} // namespace Pokemon
} // namespace PokemonAutomation
