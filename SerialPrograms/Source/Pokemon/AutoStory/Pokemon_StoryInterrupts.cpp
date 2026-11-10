/*  Story Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <format>
#include <optional>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_StoryInterrupts.h"

namespace PokemonAutomation{
namespace Pokemon{


void InterruptHandlerTable::add(InterruptKind kind, InterruptHandler handler){
    if (kind == InterruptKind::NONE || kind == InterruptKind::UNSETTLED){
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            std::format("No handler can clear {}.", interrupt_kind_name(kind))
        );
    }
    if (!m_handlers.emplace(kind, std::move(handler)).second){
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            std::format("A second interrupt handler for {}", interrupt_kind_name(kind))
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

//  Whether an UNSETTLED verdict on `look` may be a battle's opening: its
//  fade is white (an encounter's flash), still down (a battle's fade ends
//  on the battle), or came after a dialog box (a trainer's lines).
bool may_open_a_battle(const ScreenLook& look, const InterruptVerdict& verdict){
    const auto fade = std::find_if(verdict.evidence.begin(), verdict.evidence.end(), [](const ScreenEventChange& change){
        return change.edge == ScreenEventEdge::ROSE &&
            (change.event == ScreenEvent::FADE_WHITE || change.event == ScreenEvent::FADE_BLACK);
    });
    if (fade == verdict.evidence.end()){
        return false;
    }
    const bool still_down = std::none_of(verdict.evidence.begin(), verdict.evidence.end(), [](const ScreenEventChange& change){
        return change.edge == ScreenEventEdge::FELL;
    });
    if (fade->event == ScreenEvent::FADE_WHITE || still_down){
        return true;
    }
    return std::any_of(look.log.changes().begin(), look.log.changes().end(), [&](const ScreenEventChange& change){
        return change.event == ScreenEvent::DIALOG_BOX && change.edge == ScreenEventEdge::ROSE &&
            look.window.start <= change.time && change.time < fade->time;
    });
}

//  The look's verdict, looked at again while it is unsettled, up to
//  max_looks looks in all, or max_opening_looks while it may be a battle's
//  opening; not again before a step that starts on it. `screen` is how the
//  last look ended.
InterruptVerdict settled_verdict(const StoryInterrupts& interrupts, StoryStepKind step, ScreenSettle& screen){
    ScreenLook look = interrupts.look(true);
    InterruptVerdict verdict = classify_interrupt(look.log, look.window, interrupts.knobs);
    const bool home = look.log.showing().has(ScreenEvent::SWITCH_HOME_MENU);
    if (!home && !interrupts.handlers.step_starts_on(step, InterruptKind::UNSETTLED)){
        for (size_t looks = 1; verdict.kind == InterruptKind::UNSETTLED; looks++){
            const size_t limit = may_open_a_battle(look, verdict) ? interrupts.max_opening_looks : interrupts.max_looks;
            if (looks >= limit){
                break;
            }
            look = interrupts.look(false);
            verdict = classify_interrupt(look.log, look.window, interrupts.knobs);
        }
    }
    screen = judge_settle(look);
    return verdict;
}

} // namespace


ScreenSettle judge_settle(const ScreenLook& look){
    ScreenSettle out;
    const std::optional<WallClock> last = look.log.last_frame();
    if (!last || *last < look.window.start){
        return out;
    }
    out.seen = true;
    out.showing = look.log.showing();
    WallClock since = look.window.start;
    for (const ScreenEventChange& change : look.log.changes()){
        if (change.time <= look.window.end){
            since = std::max(since, change.time);
        }
    }
    const WallClock end = std::max(look.window.end, since);
    out.quiet = std::chrono::duration_cast<Milliseconds>(end - since);
    return out;
}

bool on_home_menu(const ScreenSettle& settle){
    return settle.seen && settle.showing.has(ScreenEvent::SWITCH_HOME_MENU);
}

bool is_settled(const ScreenSettle& settle){
    return settle.seen && !on_home_menu(settle) &&
        settle.showing.has(ScreenEvent::PLAYER_CAN_MOVE) &&
        !settle.showing.any_of({ScreenEvent::DIALOG_BOX, ScreenEvent::BATTLE_STARTED}) &&
        fade_in(settle.showing) == WarpFade::NONE;
}

std::string describe(const ScreenSettle& settle){
    if (!settle.seen){
        return "no frame seen";
    }
    const std::string quiet = std::format("quiet {:.1f} s", settle.quiet.count() / 1000.0);
    if (on_home_menu(settle)){
        return std::format("Switch HOME menu ({})", quiet);
    }
    if (is_settled(settle)){
        return std::format("settled overworld ({})", quiet);
    }
    return std::format("{} showing ({})", describe(settle.showing), quiet);
}


InterruptCheck clear_story_interrupt(const StoryInterrupts& interrupts, const StoryStep& step, RunState& state){
    InterruptCheck out;
    out.verdict = settled_verdict(interrupts, step.kind, out.screen);
    if (on_home_menu(out.screen)){
        out.verdict = {};
        out.outcome = outcome_game_not_running();
        return out;
    }
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
        out.outcome = outcome_failed("unhandled_interrupt", std::format("{} before {}", interrupt_kind_name(kind), step.name));
        return out;
    }
    const Outcome handled = handler->handle(out.verdict, state);
    if (!handled.succeeded()){
        out.outcome = outcome_failed("interrupt_failed", std::format("{}: {}", interrupt_kind_name(kind), to_string(handled)));
        return out;
    }
    apply_interrupt_pose(state.pose, handler->pose);
    return out;
}



} // namespace Pokemon
} // namespace PokemonAutomation
