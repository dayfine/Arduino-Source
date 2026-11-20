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

//  Nearer Pokemon stand lower on the screen, and their plates with them:
//  first-throw catches had plates at 0.49 and 0.50, a Mareep missed 11
//  times at about 15 m at 0.40. Plate width and bracket height did not
//  tell them apart.
constexpr double NEAR_PLATE_Y = 0.45;

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


namespace{

//  So a target that keeps dodging does not take all of the patrol's throws.
constexpr int THROWS_AT_ONE_TARGET = 3;

} // namespace

SpotVisit spot_visit(int spot, std::span<const int> caught_at){
    const bool caught = std::find(caught_at.begin(), caught_at.end(), spot) != caught_at.end();
    return caught ? SpotVisit::TURN_BACK : SpotVisit::CATCH;
}

AfterThrow after_throw(const ThrowSeen& seen, bool battle, const std::optional<LockTarget>& relocked, int throws_at_lock){
    if (seen.registered){
        return AfterThrow::STOP_ENDS;
    }
    if (battle){
        return AfterThrow::WEAKEN;
    }
    if (seen.new_species()){
        return AfterThrow::STOP_ENDS;
    }
    if (relocked && relocked->caught != true){
        return throws_at_lock >= THROWS_AT_ONE_TARGET ? AfterThrow::MOVE_ON : AfterThrow::THROW_AGAIN;
    }
    return AfterThrow::LOOK_AGAIN;
}

StopAim stop_aim(bool locked_on, const std::vector<LockTarget>& plates){
    if (!locked_on){
        return StopAim::MOVE_ON;
    }
    if (plates.empty()){
        return StopAim::LOOK_AGAIN;
    }
    return nearest_uncaught(plates) ? StopAim::THROW : StopAim::LOOK_FURTHER;
}

bool CameraPitch::level_now(){
    const bool tipped = m_tipped;
    m_tipped = false;
    return tipped;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
