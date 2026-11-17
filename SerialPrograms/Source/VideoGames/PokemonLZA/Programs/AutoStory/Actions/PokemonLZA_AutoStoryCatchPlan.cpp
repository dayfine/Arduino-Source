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


std::string_view LockTarget::caught_text() const{
    if (!caught){
        return "caught mark unread";
    }
    return *caught ? "caught" : "not caught";
}

namespace{

//  A plate whose bar sits above this share of the screen's height is a far
//  target: step toward it before throwing. Nearer Pokemon stand lower on
//  the screen (the ground rises toward the horizon), and their plates with
//  them. Console B, 2026-11-16: the first-throw catches at 98 and 100 had
//  their plates at 0.49 and 0.50; the Mareep missed 11 times at 60 (about
//  15 m off) at 0.40. The plate's width (0.056 at all three) and the
//  brackets' height (0.079 and 0.096 caught, 0.092 to 0.112 missed: it
//  follows the species' size too) did not tell them apart.
constexpr double NEAR_PLATE_Y = 0.45;

//  Steps toward a far target at most: before the first throw at a lock,
//  and after each miss.
constexpr int STEPS_BEFORE_FIRST_THROW = 4;
constexpr int STEPS_AFTER_MISS = 2;

} // namespace

bool step_toward_target(const LockTarget& target, int misses, int steps){
    if (target.y >= NEAR_PLATE_Y){
        return false;
    }
    return steps < (misses == 0 ? STEPS_BEFORE_FIRST_THROW : STEPS_AFTER_MISS);
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

std::optional<NewSpeciesSign> ThrowSeen::new_species() const{
    if (registered){
        return NewSpeciesSign::BANNER;
    }
    if (plate_caught == false && target_gone){
        return NewSpeciesSign::PLATE_NOT_CAUGHT;
    }
    return std::nullopt;
}

std::string_view ThrowSeen::new_species_text() const{
    const std::optional<NewSpeciesSign> sign = new_species();
    if (!sign){
        return "no new species";
    }
    if (*sign == NewSpeciesSign::BANNER){
        return "a new species: banner";
    }
    return "a new species: plate was not caught, target gone";
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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
