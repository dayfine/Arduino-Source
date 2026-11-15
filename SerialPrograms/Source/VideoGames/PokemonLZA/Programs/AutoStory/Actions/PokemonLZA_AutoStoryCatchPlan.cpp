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


SpeciesTally species_tally(const GrassCatches& patrol, int wanted){
    if (patrol.found >= wanted){
        return patrol.found_by_plate > 0 ? SpeciesTally::INFERRED : SpeciesTally::BANNERS;
    }
    if (patrol.found == 0 && patrol.throws == 0 && patrol.caught_seen > 0){
        return SpeciesTally::INFERRED;
    }
    return SpeciesTally::SHORT;
}

bool read_dex_count(Pokemon::StoryCheckLevel checks, SpeciesTally tally){
    if (tally != SpeciesTally::BANNERS){
        return true;
    }
    return checks == Pokemon::StoryCheckLevel::THOROUGH || checks == Pokemon::StoryCheckLevel::TESTING;
}

bool enough_species(SpeciesTally tally, std::optional<int> dex_count, int dex_needed){
    if (dex_count){
        return *dex_count >= dex_needed;
    }
    return tally != SpeciesTally::SHORT;
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
