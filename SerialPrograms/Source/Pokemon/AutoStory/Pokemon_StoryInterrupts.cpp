/*  Story Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_StoryInterrupts.h"

namespace PokemonAutomation{
namespace Pokemon{


void InterruptHandlerTable::add(InterruptKind kind, InterruptHandler handler){
    if (kind == InterruptKind::NONE || kind == InterruptKind::UNSETTLED){
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            std::string("No handler can clear ") + interrupt_kind_name(kind) + "."
        );
    }
    if (!m_handlers.emplace(kind, std::move(handler)).second){
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            std::string("A second interrupt handler for ") + interrupt_kind_name(kind)
        );
    }
}

const InterruptHandler* InterruptHandlerTable::find(InterruptKind kind) const{
    const auto iter = m_handlers.find(kind);
    return iter == m_handlers.end() ? nullptr : &iter->second;
}

void InterruptHandlerTable::starts_on(StoryStepKind step, InterruptKind kind){
    m_starts_on.emplace(step, kind);
}

bool InterruptHandlerTable::step_starts_on(StoryStepKind step, InterruptKind kind) const{
    if (m_starts_on.contains({step, kind})){
        return true;
    }
    return kind == InterruptKind::UNSETTLED && (
        m_starts_on.contains({step, InterruptKind::WILD_ENCOUNTER}) ||
        m_starts_on.contains({step, InterruptKind::TRAINER_SIGHT})
    );
}


namespace{

InterruptVerdict judge(const StoryInterrupts& interrupts, bool fresh){
    const ScreenLook look = interrupts.look(fresh);
    return classify_interrupt(look.log, look.window, interrupts.knobs);
}

//  The look's verdict, looked at again while it is unsettled, up to
//  max_looks looks in all; not again before a step that starts on it.
InterruptVerdict settled_verdict(const StoryInterrupts& interrupts, StoryStepKind step){
    InterruptVerdict verdict = judge(interrupts, true);
    if (interrupts.handlers.step_starts_on(step, InterruptKind::UNSETTLED)){
        return verdict;
    }
    for (size_t looks = 1; verdict.kind == InterruptKind::UNSETTLED && looks < interrupts.max_looks; looks++){
        verdict = judge(interrupts, false);
    }
    return verdict;
}

} // namespace


InterruptCheck clear_story_interrupt(const StoryInterrupts& interrupts, RunState& state, const StoryStep& step){
    InterruptCheck out;
    out.verdict = settled_verdict(interrupts, step.kind);
    const InterruptKind kind = out.verdict.kind;
    if (kind == InterruptKind::NONE){
        return out;
    }
    if (interrupts.handlers.step_starts_on(step.kind, kind)){
        out.verdict = {};
        return out;
    }
    if (kind == InterruptKind::UNSETTLED){
        out.outcome = outcome_failed("interrupt_unsettled", step.name);
        return out;
    }
    const InterruptHandler* handler = interrupts.handlers.find(kind);
    if (handler == nullptr || !handler->handle){
        out.outcome = outcome_failed("unhandled_interrupt", std::string(interrupt_kind_name(kind)) + " before " + step.name);
        return out;
    }
    const Outcome handled = handler->handle(state, out.verdict);
    if (!handled.succeeded()){
        out.outcome = outcome_failed("interrupt_failed", std::string(interrupt_kind_name(kind)) + ": " + to_string(handled));
        return out;
    }
    apply_interrupt_pose(state.pose, handler->pose);
    return out;
}



} // namespace Pokemon
} // namespace PokemonAutomation
