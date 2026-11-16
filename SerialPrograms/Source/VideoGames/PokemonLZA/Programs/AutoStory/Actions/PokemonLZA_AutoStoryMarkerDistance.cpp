/*  PokemonLZA AutoStory - Marker distance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryMarkerDistance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  A 0.6 s step covers about 3 of the marker's distance (console B,
//  2026-11-16: 109 to 42 in about 20 steps).
constexpr int STEP_COVERS = 3;

bool near(int a, int b){
    return a >= b - 12 && a <= b + 12;
}

} // namespace


bool MarkerDistance::read(int distance){
    if (m_last && !near(distance, *m_last)){
        m_rejected = m_rejected_read && near(distance, *m_rejected_read) ? m_rejected + 1 : 1;
        m_rejected_read = distance;
        if (m_rejected < 3){
            return false;
        }
    }
    m_rejected_read.reset();
    m_rejected = 0;
    m_last = distance;
    m_steps_unread = 0;
    return true;
}

void MarkerDistance::stepped(){
    m_steps_unread++;
}

std::optional<int> MarkerDistance::position() const{
    if (!m_last){
        return std::nullopt;
    }
    const int moved = STEP_COVERS * m_steps_unread;
    return m_direction == MarkerDirection::BACK ? *m_last + moved : *m_last - moved;
}

bool MarkerDistance::reached(int stop_at) const{
    const std::optional<int> at = position();
    if (!at){
        return false;
    }
    return m_direction == MarkerDirection::BACK ? *at >= stop_at : *at <= stop_at;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
