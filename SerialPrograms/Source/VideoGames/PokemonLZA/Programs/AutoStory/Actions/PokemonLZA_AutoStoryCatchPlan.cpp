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


BracketWatch::BracketWatch(std::chrono::milliseconds settle, std::chrono::milliseconds limit)
    : m_settle(settle)
    , m_limit(limit)
{}

void BracketWatch::add(std::chrono::milliseconds elapsed, const std::optional<BracketSighting>& sighting){
    m_last = std::max(m_last, elapsed);
    if (!sighting){
        return;
    }
    if (!m_first_seen){
        m_first_seen = elapsed;
    }
    m_sightings.push_back(*sighting);
}

bool BracketWatch::done() const{
    if (m_last >= m_limit){
        return true;
    }
    return m_first_seen && m_last - *m_first_seen >= m_settle;
}

std::optional<BracketSighting> BracketWatch::nearest() const{
    std::optional<BracketSighting> best;
    for (const BracketSighting& sighting : m_sightings){
        if (!best || sighting.height > best->height){
            best = sighting;
        }
    }
    return best;
}


CameraTurn camera_turn_toward(double x){
    const double off = x - 0.5;
    CameraTurn turn;
    if (std::abs(off) <= 0.06){
        return turn;
    }
    turn.stick_x = off > 0 ? 0.4 : -0.4;
    turn.time = std::chrono::milliseconds(std::lround(std::min(1.0, std::abs(off) * 4) * 400) + 80);
    return turn;
}


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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
