/*  PokemonLZA AutoStory - Wild Zone 1 road
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <functional>
#include "PokemonLZA_AutoStoryZone1Road.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::vector<int> alternating_stops(std::span<const int> spots, size_t visits){
    std::vector<int> stops;
    if (spots.empty()){
        return stops;
    }
    for (size_t i = 0; i < visits; i++){
        stops.push_back(spots[i % spots.size()]);
    }
    return stops;
}

std::vector<int> road_stops_to_gate(std::span<const int> spots, std::optional<int> distance){
    std::vector<int> stops(spots.begin(), spots.end());
    std::sort(stops.begin(), stops.end(), std::greater<int>());
    if (!distance || stops.empty()){
        return stops;
    }
    std::vector<int> ahead;
    for (int stop : stops){
        if (stop < *distance){
            ahead.push_back(stop);
        }
    }
    if (ahead.empty()){
        ahead.push_back(stops.back());
    }
    return ahead;
}


namespace{

bool near(int a, int b){
    return a >= b - 12 && a <= b + 12;
}

//  Agreeing reads in a row far from the last read taken that replace it.
constexpr size_t FAR_READS_TAKEN = 3;
//  Reads taken in a row with none nearer that make a walk held up, and
//  the most held-up sidesteps.
constexpr size_t HELD_UP_READS = 4;
constexpr size_t MAX_SIDESTEPS = 4;

} // namespace


RoadLook RoadWalkReads::next(std::optional<int> distance){
    if (!distance || !take(*distance)){
        return RoadLook::STEP;
    }
    if (reached()){
        return RoadLook::ARRIVED;
    }
    if (m_direction == MarkerDirection::BACK){
        return RoadLook::STEP;
    }
    return held_up(*distance);
}

bool RoadWalkReads::take(int distance){
    const bool near_last = m_last && near(distance, *m_last);
    if (m_floor && distance < *m_floor && !near_last){
        return false;
    }
    if (m_last && !near_last){
        m_far_reads = m_far_read && near(distance, *m_far_read) ? m_far_reads + 1 : 1;
        m_far_read = distance;
        if (m_far_reads < FAR_READS_TAKEN){
            return false;
        }
    }
    m_far_read.reset();
    m_far_reads = 0;
    m_last = distance;
    return true;
}

bool RoadWalkReads::reached() const{
    return m_direction == MarkerDirection::BACK ? *m_last >= m_stop_at : *m_last <= m_stop_at;
}

RoadLook RoadWalkReads::held_up(int distance){
    if (!m_best || distance < *m_best){
        m_best = distance;
        m_unmoved_reads = 0;
        return RoadLook::STEP;
    }
    if (++m_unmoved_reads < HELD_UP_READS){
        return RoadLook::STEP;
    }
    m_unmoved_reads = 0;
    if (m_sidesteps >= MAX_SIDESTEPS){
        return RoadLook::STEP;
    }
    m_sidesteps++;
    return m_sidesteps % 2 == 1 ? RoadLook::SIDESTEP_RIGHT : RoadLook::SIDESTEP_LEFT;
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
