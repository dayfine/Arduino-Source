/*  Program Transition
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "ProgramTransition.h"

namespace PokemonAutomation{


void ProgramTransitionLatch::started(ActionSource source, std::optional<std::string> request_id, WallClock now){
    std::lock_guard<Mutex> lg(m_lock);
    m_stop_requested = false;
    m_error = false;
    m_outcome = {.start_request_id = request_id};
    m_last = ProgramTransition{ProgramAction::START, source, std::move(request_id), now};
}
void ProgramTransitionLatch::stop_requested(ActionSource source, std::optional<std::string> request_id, WallClock now){
    std::lock_guard<Mutex> lg(m_lock);
    m_stop_requested = true;
    m_last = ProgramTransition{ProgramAction::STOP, source, std::move(request_id), now};
}
void ProgramTransitionLatch::error_reported(std::string_view message){
    std::lock_guard<Mutex> lg(m_lock);
    m_error = true;
    if (m_outcome.kind != ProgramOutcomeKind::START_CHECK_FAILED){
        m_outcome.message = message;
    }
}
void ProgramTransitionLatch::start_check_failed(std::string check, std::string message, WallClock at){
    std::lock_guard<Mutex> lg(m_lock);
    m_error = true;
    m_outcome.kind = ProgramOutcomeKind::START_CHECK_FAILED;
    m_outcome.check = std::move(check);
    m_outcome.message = std::move(message);
    m_outcome.at = at;
}
void ProgramTransitionLatch::set_checkpoint(std::string checkpoint){
    std::lock_guard<Mutex> lg(m_lock);
    m_outcome.checkpoint = std::move(checkpoint);
}
void ProgramTransitionLatch::finished(WallClock now, ProgramOutcomeKind kind){
    std::lock_guard<Mutex> lg(m_lock);
    if (m_stop_requested){
        kind = ProgramOutcomeKind::CANCELLED;
    }
    if (kind != ProgramOutcomeKind::FAILED || m_outcome.kind != ProgramOutcomeKind::START_CHECK_FAILED){
        m_outcome.kind = kind;
        m_outcome.at = now;
        m_outcome.check.clear();
    }
    if (kind != ProgramOutcomeKind::FAILED){
        m_outcome.message.clear();
    }
    if (!m_stop_requested){
        const ActionSource source = m_error || kind == ProgramOutcomeKind::FAILED
            ? ActionSource::PROGRAM_ERROR : ActionSource::PROGRAM;
        m_last = ProgramTransition{ProgramAction::STOP, source, std::nullopt, now};
    }
    m_last->outcome = std::move(m_outcome);
}
std::optional<ProgramTransition> ProgramTransitionLatch::last() const{
    std::lock_guard<Mutex> lg(m_lock);
    return m_last;
}



}  // namespace PokemonAutomation
