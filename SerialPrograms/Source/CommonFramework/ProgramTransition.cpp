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
    m_last = ProgramTransition{ProgramAction::START, source, std::move(request_id), now};
}
void ProgramTransitionLatch::stop_requested(ActionSource source, std::optional<std::string> request_id, WallClock now){
    std::lock_guard<Mutex> lg(m_lock);
    m_stop_requested = true;
    m_last = ProgramTransition{ProgramAction::STOP, source, std::move(request_id), now};
}
void ProgramTransitionLatch::error_reported(){
    std::lock_guard<Mutex> lg(m_lock);
    m_error = true;
}
void ProgramTransitionLatch::finished(WallClock now){
    std::lock_guard<Mutex> lg(m_lock);
    if (m_stop_requested){
        return;
    }
    const ActionSource source = m_error ? ActionSource::PROGRAM_ERROR : ActionSource::PROGRAM;
    m_last = ProgramTransition{ProgramAction::STOP, source, std::nullopt, now};
}
std::optional<ProgramTransition> ProgramTransitionLatch::last() const{
    std::lock_guard<Mutex> lg(m_lock);
    return m_last;
}



}  // namespace PokemonAutomation
