/*  PokemonLZA AutoStory - Bracket watch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include "PokemonLZA_AutoStoryBracketWatch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
