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

//  How many reads not taken a read must agree with to be taken anyway,
//  and how many are kept.
constexpr size_t AGREEING_UNTAKEN = 2;
constexpr size_t UNTAKEN_KEPT = 3;

} // namespace


int MarkerDistance::moved_on(const Untaken& read) const{
    const int moved = STEP_COVERS * (m_steps - read.step);
    return m_direction == MarkerDirection::BACK ? read.distance + moved : read.distance - moved;
}

bool MarkerDistance::read(int distance){
    const std::optional<int> at = position();
    if (!at || !near(distance, *at)){
        size_t agreeing = 0;
        for (const Untaken& untaken : m_untaken){
            //  A read that, moved on, puts the walk at or past the
            //  objective agrees with nothing.
            const int predicted = moved_on(untaken);
            if (predicted > 0 && near(distance, predicted)){
                agreeing++;
            }
        }
        if (agreeing < AGREEING_UNTAKEN){
            if (m_untaken.size() == UNTAKEN_KEPT){
                m_untaken.erase(m_untaken.begin());
            }
            m_untaken.push_back(Untaken{.distance = distance, .step = m_steps});
            return false;
        }
    }
    m_untaken.clear();
    m_last = distance;
    m_steps_unread = 0;
    return true;
}

void MarkerDistance::stepped(){
    m_steps++;
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
