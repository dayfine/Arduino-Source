/*  Story Action Registry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checks and runs a story step with its game's action.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActionRegistry_H
#define PokemonAutomation_Pokemon_StoryActionRegistry_H

#include <format>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon_StoryGame.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The failure of a console_only action (StoryActionSpec) on a backend that
//  is not a console.
inline constexpr std::string_view CONSOLE_ONLY = "console_only";

//  What is wrong with `checkpoint` before it runs: no id, empty or repeated
//  step names, actions `game` does not have, args that don't fit a spec.
template <StoryPose PoseType>
std::vector<std::string> lint_story_checkpoint(const StoryGame<PoseType>& game, const StoryCheckpoint& checkpoint);

//  Runs `step`'s action with its args' fallbacks filled in, then logs a
//  "[Step]" line. A step with no action, or a console_only action on another
//  backend, fails before any input.
template <StoryPose PoseType>
Outcome run_story_step(const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend, const StoryStep& step, RunState& state);


namespace StoryActionInternal{

//  The action of `kind` named `name`; for an empty name, the kind's first.
template <StoryPose PoseType>
const StoryAction<PoseType>* find_action(const StoryGame<PoseType>& game, StoryStepKind kind, std::string_view name){
    for (const StoryAction<PoseType>& action : game.actions){
        if (action.spec.kind == kind && (name.empty() || action.spec.name == name)){
            return &action;
        }
    }
    return nullptr;
}

} // namespace StoryActionInternal


template <StoryPose PoseType>
std::vector<std::string> lint_story_checkpoint(const StoryGame<PoseType>& game, const StoryCheckpoint& checkpoint){
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
        const StoryAction<PoseType>* action = StoryActionInternal::find_action(game, step.kind, step.action);
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

template <StoryPose PoseType>
Outcome run_story_step(const StoryGame<PoseType>& game, StoryBackend<PoseType>& backend, const StoryStep& step, RunState& state){
    backend.scope().throw_if_cancelled();
    const std::string_view action_name = step.action.empty() ? std::string_view("default") : std::string_view(step.action);
    const StoryAction<PoseType>* action = StoryActionInternal::find_action(game, step.kind, step.action);
    Outcome outcome;
    if (action == nullptr || !action->handler){
        outcome = outcome_failed("unsupported_step", std::format(
            "{} {}: the game has no action for it", story_step_kind_name(step.kind), action_name
        ));
    }else if (action->spec.console_only && backend.kind() != StoryBackendKind::CONSOLE){
        outcome = outcome_failed(std::string(CONSOLE_ONLY), std::format(
            "{} runs only on a console, not on a {} backend", action->spec.name, story_backend_kind_name(backend.kind())
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
#endif
