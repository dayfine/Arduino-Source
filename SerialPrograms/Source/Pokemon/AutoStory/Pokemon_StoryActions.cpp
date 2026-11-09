/*  Story Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_StoryVariant.h"
#include "Pokemon_StoryActions.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

const StoryAction* find_in(const std::map<std::string, StoryAction, std::less<>>& actions, std::string_view name){
    const auto iter = actions.find(name);
    return iter == actions.end() ? nullptr : &iter->second;
}

} // namespace


void StoryActionTable::add(StoryStepKind kind, std::string name, StoryAction action){
    const std::string key = std::format("{} '{}'", story_step_kind_name(kind), name);
    if (!m_by_kind[kind].emplace(std::move(name), std::move(action)).second){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "A second story action for " + key);
    }
}

void StoryActionTable::add_any_kind(std::string name, StoryAction action){
    if (name.empty()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "A story action for any kind needs a name.");
    }
    const std::string key = name;
    if (!m_any_kind.emplace(std::move(name), std::move(action)).second){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "A second story action for any kind named " + key);
    }
}

const StoryAction* StoryActionTable::find(StoryStepKind kind, std::string_view name) const{
    const auto kind_iter = m_by_kind.find(kind);
    if (kind_iter != m_by_kind.end()){
        if (const StoryAction* exact = find_in(kind_iter->second, name)){
            return exact;
        }
    }
    if (const StoryAction* any_kind = find_in(m_any_kind, name)){
        return any_kind;
    }
    if (kind_iter != m_by_kind.end()){
        return find_in(kind_iter->second, "");
    }
    return nullptr;
}

const StoryAction* StoryActionTable::find(const StoryStep& step) const{
    return find(step.kind, variant_name(step.variant));
}


std::vector<std::string> lint_story_checkpoint(const StoryCheckpoint& data, const StoryActionTable& table){
    std::vector<std::string> problems;
    for (size_t c = 0; c < data.steps.size(); c++){
        const StoryStep& step = data.steps[c];
        if (table.find(step) == nullptr){
            problems.emplace_back(std::format(
                "{} step {} {}: no action for {} '{}'",
                data.id, c + 1, step.name, story_step_kind_name(step.kind), variant_name(step.variant)
            ));
        }
    }
    return problems;
}


void StepGuard::before_step(const StoryStep&){}

bool StepGuard::stops_on_failure(const StoryStep&) const{
    return false;
}

void StepGuard::on_failure(const StoryStep&, const OperationFailedException&){}


TableExecutor::TableExecutor(const StoryActionTable& table, StepGuard& guard)
    : m_table(table)
    , m_guard(guard)
{}

Outcome TableExecutor::run(RunState& state, const StoryStep& step){
    m_guard.before_step(step);
    try{
        if (const StoryAction* action = m_table.find(step)){
            return action->act(state, step);
        }
        return StoryStepExecutor::run(state, step);
    }catch (OperationFailedException& e){
        if (m_guard.stops_on_failure(step)){
            throw;
        }
        m_guard.on_failure(step, e);
        return outcome_failed("operation_failed", e.message());
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
