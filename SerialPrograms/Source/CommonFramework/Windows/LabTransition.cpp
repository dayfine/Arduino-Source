/*  Lab Transition
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <random>
#include "LabTransition.h"

namespace PokemonAutomation{


std::string new_lab_uuid(){
    static thread_local std::mt19937_64 generator(std::random_device{}());
    const uint64_t high = (generator() & 0xffffffffffff0fffull) | 0x0000000000004000ull;
    const uint64_t low = (generator() & 0x3fffffffffffffffull) | 0x8000000000000000ull;
    return std::format(
        "{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
        high >> 32, (high >> 16) & 0xffff, high & 0xffff,
        low >> 48, low & 0xffffffffffffull
    );
}

bool is_lab_uuid(std::string_view text){
    if (text.size() != 36){
        return false;
    }
    for (size_t c = 0; c < text.size(); c++){
        const char ch = text[c];
        if (c == 8 || c == 13 || c == 18 || c == 23){
            if (ch != '-'){
                return false;
            }
        }else if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))){
            return false;
        }
    }
    return true;
}



void LabTransitionTracker::expect_lab(LabTransitionAction action, std::string request_id){
    std::lock_guard<Mutex> lg(m_lock);
    m_expected = Expected{action, std::move(request_id)};
}
void LabTransitionTracker::cancel_lab(){
    std::lock_guard<Mutex> lg(m_lock);
    m_expected.reset();
}

LabTransitionSource LabTransitionTracker::source_of(LabTransitionAction action, std::optional<std::string>& request_id){
    if (!m_expected || m_expected->action != action){
        return LabTransitionSource::UI;
    }
    request_id = std::move(m_expected->request_id);
    m_expected.reset();
    return LabTransitionSource::LAB;
}

void LabTransitionTracker::on_state_change(ProgramState state, WallClock now){
    std::lock_guard<Mutex> lg(m_lock);
    const ProgramState previous = m_state;
    m_state = state;
    std::optional<std::string> request_id;
    switch (state){
    case ProgramState::RUNNING:{
        m_error = false;
        const LabTransitionSource source = source_of(LabTransitionAction::START, request_id);
        m_last = LabTransition{LabTransitionAction::START, source, std::move(request_id), now};
        return;
    }
    case ProgramState::STOPPING:{
        const LabTransitionSource source = source_of(LabTransitionAction::STOP, request_id);
        m_last = LabTransition{LabTransitionAction::STOP, source, std::move(request_id), now};
        return;
    }
    case ProgramState::STOPPED:
        //  After STOPPING the stop is already named. Straight from RUNNING,
        //  nobody asked: the program ended by itself.
        if (previous == ProgramState::RUNNING){
            const LabTransitionSource source = m_error ? LabTransitionSource::PROGRAM_ERROR : LabTransitionSource::PROGRAM;
            m_last = LabTransition{LabTransitionAction::STOP, source, std::nullopt, now};
        }
        return;
    case ProgramState::NOT_READY:
        return;
    }
}
void LabTransitionTracker::on_error(){
    std::lock_guard<Mutex> lg(m_lock);
    m_error = true;
}
void LabTransitionTracker::reset_program(ProgramState state){
    std::lock_guard<Mutex> lg(m_lock);
    m_state = state;
    m_error = false;
    m_expected.reset();
}
std::optional<LabTransition> LabTransitionTracker::last() const{
    std::lock_guard<Mutex> lg(m_lock);
    return m_last;
}



}  // namespace PokemonAutomation
