/*  PokemonLZA AutoStory - Marker distance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <iterator>
#include "PokemonLZA_AutoStoryMarkerDistance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

bool near(int a, int b){
    return a >= b - 12 && a <= b + 12;
}

//  How many reads not taken a read must agree with to be taken anyway,
//  and how many are kept.
constexpr size_t AGREEING_UNTAKEN = 2;
constexpr size_t UNTAKEN_KEPT = 3;

//  The looks in a row with no read taken that each new view gets, and the
//  views in turn: the camera nudges, then the sidesteps; after the last,
//  the walk is blocked.
constexpr size_t LOOKS_PER_VIEW = 3;
constexpr MarkerMove CAMERA_NUDGES[] = {
    MarkerMove::NUDGE_CAMERA_LEFT,
    MarkerMove::NUDGE_CAMERA_RIGHT,
};
//  The lanes (MarkerDistance::lane()) the sidesteps go to in turn, around
//  the marker's line rather than from where the walk stands, so a walk
//  does not drift to one side.
constexpr int SIDESTEP_LANES[] = {-1, 1, -2, 2};
constexpr size_t NEW_VIEWS = std::size(CAMERA_NUDGES) + std::size(SIDESTEP_LANES);

//  Reads taken in a row with none further on that make a walk held up,
//  and how much further on it then is before it leaves the held-up lane.
constexpr size_t HELD_UP_READS = 4;
constexpr int HELD_UP_CLEARED = 6;

} // namespace


bool MarkerDistance::read(int distance){
    if (m_floor && distance < *m_floor){
        return false;
    }
    if (!m_last || !near(distance, *m_last)){
        size_t agreeing = 0;
        for (int untaken : m_untaken){
            if (near(distance, untaken)){
                agreeing++;
            }
        }
        if (agreeing < AGREEING_UNTAKEN){
            if (m_untaken.size() == UNTAKEN_KEPT){
                m_untaken.erase(m_untaken.begin());
            }
            m_untaken.push_back(distance);
            return false;
        }
    }
    m_untaken.clear();
    m_last = distance;
    return true;
}

MarkerMove MarkerDistance::next(std::optional<int> distance, int stop_at){
    if (!distance || !read(*distance)){
        return unread_move();
    }
    m_unread_looks = 0;
    if (reached(stop_at)){
        m_lane = 0;
        return MarkerMove::ARRIVED;
    }
    if (!m_best || progress(*m_best, *m_last) > 0){
        m_best = m_last;
        m_unmoved_reads = 0;
    }else if (++m_unmoved_reads >= HELD_UP_READS && m_held_ups < std::size(SIDESTEP_LANES)){
        m_unmoved_reads = 0;
        m_held_at = m_best;
        return sidestep_to(SIDESTEP_LANES[m_held_ups++]);
    }
    if (m_held_at && progress(*m_held_at, *m_best) >= HELD_UP_CLEARED){
        m_held_at.reset();
    }
    if (!m_held_at){
        m_lane = 0;
    }
    return MarkerMove::STEP;
}

MarkerMove MarkerDistance::unread_move(){
    m_unread_looks++;
    const size_t views = m_unread_looks / LOOKS_PER_VIEW;
    if (m_unread_looks % LOOKS_PER_VIEW != 0){
        //  Between new views: a nudged camera stays, else re-centered.
        return views > 0 && views <= std::size(CAMERA_NUDGES) ? MarkerMove::REREAD : MarkerMove::RECENTER;
    }
    if (views <= std::size(CAMERA_NUDGES)){
        return CAMERA_NUDGES[views - 1];
    }
    if (views > NEW_VIEWS){
        m_lane = 0;
        return MarkerMove::BLOCKED;
    }
    m_held_at.reset();
    return sidestep_to(SIDESTEP_LANES[views - 1 - std::size(CAMERA_NUDGES)]);
}

MarkerMove MarkerDistance::sidestep_to(int lane){
    const bool left = lane < m_lane;
    m_lane = lane;
    return left ? MarkerMove::SIDESTEP_LEFT : MarkerMove::SIDESTEP_RIGHT;
}

int MarkerDistance::progress(int from, int to) const{
    return m_direction == MarkerDirection::BACK ? to - from : from - to;
}

bool MarkerDistance::reached(int stop_at) const{
    if (m_direction == MarkerDirection::BACK){
        return *m_last >= stop_at;
    }
    return *m_last <= std::max(stop_at, m_floor.value_or(stop_at));
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
