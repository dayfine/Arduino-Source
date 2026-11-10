/*  Story Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Exceptions.h"
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
    return find(step.kind, step.action);
}


std::vector<std::string> lint_story_checkpoint(const StoryCheckpoint& data, const StoryActionTable& table){
    std::vector<std::string> problems;
    for (size_t c = 0; c < data.steps.size(); c++){
        const StoryStep& step = data.steps[c];
        const StoryAction* action = table.find(step);
        const std::vector<std::string> errors = action == nullptr
            ? std::vector<std::string>{"no action"}
            : story_args_errors(StoryActionSpec{.kind = step.kind, .name = step.action, .args = action->args}, step.args);
        for (const std::string& error : errors){
            problems.emplace_back(std::format(
                "{} step {} {}: {} for {} '{}'",
                data.id, c + 1, step.name, error, story_step_kind_name(step.kind), step.action
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

void StepGuard::on_interrupt(const StoryStep&, const InterruptCheck&){}

void StepGuard::on_screen(const StoryStep&, const ScreenSettle&){}


TableExecutor::TableExecutor(const StoryActionTable& table, StepGuard& guard)
    : m_table(table)
    , m_guard(guard)
{}

Outcome TableExecutor::run(RunState& state, const StoryStep& step){
    m_guard.before_step(step);
    size_t cleared = 0;
    while (true){
        if (std::optional<Outcome> failed = clear_interrupts(state, step, cleared)){
            return *failed;
        }
        try{
            Outcome outcome = run_action(state, step);
            if (outcome.succeeded() || !cleared_after_failure(state, step, cleared)){
                return outcome;
            }
        }catch (OperationFailedException& e){
            if (cleared_after_failure(state, step, cleared)){
                continue;
            }
            if (m_guard.stops_on_failure(step)){
                throw;
            }
            m_guard.on_failure(step, e);
            return outcome_failed("operation_failed", e.message());
        }
    }
}

Outcome TableExecutor::run_action(RunState& state, const StoryStep& step){
    if (const StoryAction* action = m_table.find(step)){
        return action->act(state, step);
    }
    return unsupported_step(step);
}

//  Clears what interrupted the player before `step`, counting it in
//  `cleared`. Nothing when there was nothing or it is cleared; else the
//  step's failure.
std::optional<Outcome> TableExecutor::clear_interrupts(RunState& state, const StoryStep& step, size_t& cleared){
    if (m_interrupts == nullptr){
        return std::nullopt;
    }
    InterruptCheck check = clear_story_interrupt(*m_interrupts, state, step);
    m_guard.on_screen(step, check.screen);
    if (check.verdict.kind == InterruptKind::NONE){
        //  Nothing interrupted; failed only when the game is not running.
        return check.outcome.succeeded() ? std::nullopt : std::optional<Outcome>(check.outcome);
    }
    if (check.outcome.succeeded() && ++cleared > m_interrupts->max_per_step){
        check.outcome = outcome_failed("too_many_interrupts", step.name);
    }
    m_guard.on_interrupt(step, check);
    if (!check.outcome.succeeded()){
        return check.outcome;
    }
    return std::nullopt;
}

//  Whether an interrupt explains the failure of `step`: one showed after
//  it failed and is cleared, so the step runs again.
bool TableExecutor::cleared_after_failure(RunState& state, const StoryStep& step, size_t& cleared){
    const size_t before = cleared;
    return !clear_interrupts(state, step, cleared) && cleared > before;
}



} // namespace Pokemon
} // namespace PokemonAutomation
