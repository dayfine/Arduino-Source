/*  PokemonLZA AutoStory - Zone search
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <iterator>
#include "PokemonLZA_AutoStoryZoneSearch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The ways out of a round, in turn, from the search spot (100 from the
//  objective): away first, as toward is the far gate (~55). Both ways are
//  along the road: the search stays on it (the user, 2026-11-16).
constexpr ZoneLeg OUT_LEGS[] = {ZoneLeg::AWAY, ZoneLeg::TOWARD};

void apply(ZoneOffset& offset, ZoneLeg leg){
    switch (leg){
    case ZoneLeg::TOWARD:
        offset.toward++;
        return;
    case ZoneLeg::AWAY:
        offset.toward--;
        return;
    case ZoneLeg::LEFT:
        offset.right--;
        return;
    case ZoneLeg::RIGHT:
        offset.right++;
        return;
    }
}

//  The leg that takes `offset` one leg nearer the start spot, if it is
//  off it: along the objective's line first.
std::optional<ZoneLeg> leg_back(const ZoneOffset& offset){
    if (offset.toward > 0){
        return ZoneLeg::AWAY;
    }
    if (offset.toward < 0){
        return ZoneLeg::TOWARD;
    }
    if (offset.right > 0){
        return ZoneLeg::LEFT;
    }
    if (offset.right < 0){
        return ZoneLeg::RIGHT;
    }
    return std::nullopt;
}

} // namespace


ZoneMove ZoneSearch::look(ZoneLook look){
    switch (look){
    case ZoneLook::WILD_BATTLE:
        m_step = 0;
        return ZoneMove::FIGHT;
    case ZoneLook::NOT_CAUGHT:
        m_step = 0;
        return ZoneMove::CATCH;
    case ZoneLook::NO_PLATE:
    case ZoneLook::ALL_CAUGHT:
        break;
    }
    if (++m_step < m_steps_per_turn){
        return ZoneMove::TURN;
    }
    m_step = 0;
    if (m_outs >= std::size(OUT_LEGS) && !leg_back(m_offset)){
        m_outs = 0;
        return ZoneMove::PAUSE;
    }
    return ZoneMove::WALK;
}

ZoneLeg ZoneSearch::leg(std::optional<int> distance){
    const bool floored = distance && *distance <= TOWARD_FLOOR_DISTANCE;
    std::optional<ZoneLeg> leg = leg_back(m_offset);
    if (!leg){
        leg = OUT_LEGS[m_outs % std::size(OUT_LEGS)];
        m_outs++;
    }
    if (floored && *leg == ZoneLeg::TOWARD){
        leg = ZoneLeg::AWAY;
    }
    apply(m_offset, *leg);
    return *leg;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
