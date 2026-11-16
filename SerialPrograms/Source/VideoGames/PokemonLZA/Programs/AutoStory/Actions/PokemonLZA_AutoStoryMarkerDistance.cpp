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
//  views in turn; after the last, the walk is blocked.
constexpr size_t LOOKS_PER_VIEW = 3;
constexpr MarkerMove NEW_VIEWS[] = {
    MarkerMove::NUDGE_CAMERA_LEFT,
    MarkerMove::NUDGE_CAMERA_RIGHT,
    MarkerMove::SIDESTEP_LEFT,
    MarkerMove::SIDESTEP_RIGHT,
    MarkerMove::SIDESTEP_LEFT,
    MarkerMove::SIDESTEP_RIGHT,
};

bool is_nudge(MarkerMove move){
    return move == MarkerMove::NUDGE_CAMERA_LEFT || move == MarkerMove::NUDGE_CAMERA_RIGHT;
}

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
    return reached(stop_at) ? MarkerMove::ARRIVED : MarkerMove::STEP;
}

MarkerMove MarkerDistance::unread_move(){
    m_unread_looks++;
    const size_t views = m_unread_looks / LOOKS_PER_VIEW;
    if (m_unread_looks % LOOKS_PER_VIEW != 0){
        //  Between new views: a nudged camera stays, else re-centered.
        return views > 0 && views <= std::size(NEW_VIEWS) && is_nudge(NEW_VIEWS[views - 1])
            ? MarkerMove::REREAD
            : MarkerMove::RECENTER;
    }
    return views <= std::size(NEW_VIEWS) ? NEW_VIEWS[views - 1] : MarkerMove::BLOCKED;
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
