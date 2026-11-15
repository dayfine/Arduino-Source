/*  Step Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <utility>
#include "Common/Cpp/PrettyPrint.h"
#include "StepTrace.h"

namespace PokemonAutomation{


StepTrace::StepTrace(std::string label, std::function<void(const std::string& line)> log)
    : m_log(std::move(log))
    , m_label(std::move(label))
{}

void StepTrace::screen(const std::string& name, WallClock when){
    if (!m_start){
        m_start = when;
    }
    if (m_count > 0 && name == m_last){
        return;
    }
    m_last = name;
    m_count++;
    if (!m_log){
        return;
    }
    const double seconds = std::chrono::duration<double>(when - *m_start).count();
    m_log(std::format("[{}] screen {}: {} at {}s", m_label, m_count, name, tostr_fixed(seconds, 1)));
}



} // namespace PokemonAutomation
