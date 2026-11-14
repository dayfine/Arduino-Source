/*  PokemonLZA AutoStory - Catch plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include "PokemonLZA_AutoStoryCatchPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


bool lock_target_far(const LockTarget& target){
    return target.y < FAR_PLATE_Y;
}

bool same_lock_target(const LockTarget& a, const LockTarget& b){
    return std::abs(a.x - b.x) <= 0.03 && std::abs(a.y - b.y) <= 0.03;
}

std::optional<size_t> nearest_uncaught(const std::vector<LockTarget>& targets){
    std::optional<size_t> best;
    for (size_t i = 0; i < targets.size(); i++){
        if (targets[i].caught == true){
            continue;
        }
        if (!best || targets[i].y > targets[*best].y){
            best = i;
        }
    }
    return best;
}


LockTargetCycle::LockTargetCycle(size_t max_views)
    : m_max_views(max_views)
{}

bool LockTargetCycle::add(const LockTarget& target){
    if (m_done){
        return false;
    }
    if (!m_targets.empty() && same_lock_target(target, m_targets.back())){
        //  The flick found nothing further that way.
        m_done = true;
        return false;
    }
    if (m_targets.size() > 1 && same_lock_target(target, m_targets.front())){
        //  Round to the first again.
        m_current = 0;
        m_done = true;
        return false;
    }
    m_targets.push_back(target);
    m_current = m_targets.size() - 1;
    m_done = m_targets.size() >= m_max_views;
    return !m_done;
}

int LockTargetCycle::flicks_to(size_t index) const{
    return (int)index - (int)m_current;
}


std::vector<int> patrol_stops(int near, int far, int step, int laps){
    std::vector<int> stops{near};
    if (step <= 0 || far <= near){
        return stops;
    }
    for (int lap = 0; lap < laps; lap++){
        for (int d = near + step; d < far + step; d += step){
            stops.push_back(std::min(d, far));
        }
        for (int d = far - step; d > near - step; d -= step){
            stops.push_back(std::max(d, near));
        }
    }
    return stops;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
