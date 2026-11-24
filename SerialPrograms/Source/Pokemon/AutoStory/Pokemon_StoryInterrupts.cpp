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
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonTools/ScreenEvents/ScreenFades.h"
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

void InterruptHandlerTable::starts_on(std::string action, InterruptKind kind){
    m_starts_on.emplace(std::move(action), kind);
}

bool InterruptHandlerTable::action_starts_on(std::string_view action, InterruptKind kind) const{
    const auto has = [&](InterruptKind target){
        return m_starts_on.find(std::pair<std::string, InterruptKind>{std::string(action), target}) != m_starts_on.end();
    };
    if (has(kind)){
        return true;
    }
    return kind == InterruptKind::UNSETTLED && (
        has(InterruptKind::WILD_ENCOUNTER) ||
        has(InterruptKind::TRAINER_SIGHT)
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
InterruptVerdict settled_verdict(const StoryInterrupts& interrupts, std::string_view action, ScreenSettle& screen){
    ScreenLook look = interrupts.look(true);
    InterruptVerdict verdict = classify_interrupt(look.log, look.window, interrupts.knobs);
    const bool home = look.log.showing().has(ScreenEvent::SWITCH_HOME_MENU);
    if (!home && !interrupts.handlers.action_starts_on(action, InterruptKind::UNSETTLED)){
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

//  The handler's failure, if any. A handler drives the console, which it can
//  lose: that is a failed step too, not an exception for the caller.
std::optional<StoryFailure> run_handler(const InterruptHandler& handler, const InterruptVerdict& verdict){
    try{
        return handler.handle(verdict);
    }catch (OperationFailedException& e){
        return StoryFailure{.code = "operation_failed", .detail = e.message()};
    }
}

//  What a look found after `cleared` was cleared. A battle that follows a
//  dialog is its trainer's: the lines were in the look before.
InterruptKind after_cleared(InterruptKind cleared, InterruptKind found){
    if (cleared == InterruptKind::DIALOG && found == InterruptKind::WILD_ENCOUNTER){
        return InterruptKind::TRAINER_SIGHT;
    }
    return found;
}

//  Clears `verdict` with its handler; why it could not, if it could not.
std::optional<StoryFailure> clear_one(
    const InterruptHandlerTable& handlers, std::string_view step_name, const InterruptVerdict& verdict
){
    const InterruptKind kind = verdict.kind;
    if (kind == InterruptKind::UNSETTLED){
        return StoryFailure{.code = "interrupt_unsettled", .detail = std::string(step_name)};
    }
    const InterruptHandler* handler = handlers.find(kind);
    if (handler == nullptr || !handler->handle){
        return StoryFailure{
            .code = "unhandled_interrupt",
            .detail = std::format("{} before {}", interrupt_kind_name(kind), step_name),
        };
    }
    const std::optional<StoryFailure> handled = run_handler(*handler, verdict);
    if (handled){
        return StoryFailure{
            .code = "interrupt_failed",
            .detail = std::format("{}: {}", interrupt_kind_name(kind), *handled),
        };
    }
    return std::nullopt;
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
    return std::format("{} showing ({})", settle.showing, quiet);
}


InterruptCheck clear_story_interrupt(
    const StoryInterrupts& interrupts, std::string_view step_name, std::string_view action
){
    InterruptCheck out;
    for (size_t cleared = 0;; cleared++){
        InterruptVerdict verdict = settled_verdict(interrupts, action, out.screen);
        if (on_home_menu(out.screen)){
            out.verdict = {};
            out.failure = game_not_running_failure();
            return out;
        }
        if (cleared > 0){
            verdict.kind = after_cleared(out.verdict.kind, verdict.kind);
        }
        if (verdict.kind == InterruptKind::NONE || interrupts.handlers.action_starts_on(action, verdict.kind)){
            return out;
        }
        out.verdict = std::move(verdict);
        if (cleared >= interrupts.max_per_step){
            out.failure = StoryFailure{
                .code = "too_many_interrupts",
                .detail = std::format("{} before {}", interrupt_kind_name(out.verdict.kind), step_name),
            };
            return out;
        }
        out.failure = clear_one(interrupts.handlers, step_name, out.verdict);
        if (out.failure){
            return out;
        }
        out.pose_lost = out.pose_lost || interrupts.handlers.find(out.verdict.kind)->pose == InterruptPose::LOST;
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
