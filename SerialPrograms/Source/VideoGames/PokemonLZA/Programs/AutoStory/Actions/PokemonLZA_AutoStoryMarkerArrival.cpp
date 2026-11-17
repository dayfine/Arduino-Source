/*  PokemonLZA AutoStory - Marker arrival
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryMarkerArrival.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  How near the marker a prompt or a dialog is the objective's: the lab
//  gate's scene took the marker away after 13 (console B, 2026-11-14), a
//  stray prompt on the street came at 44 or more to go.
constexpr int MARKER_NEAR = 20;
//  How near the marker a prompt or a dialog with the marker still up is
//  the objective's (MarkerArrival::arrived()); also the least read taken
//  first (MarkerArrival::look()).
constexpr int MARKER_PROMPT_NEAR = 10;

} // namespace


bool MarkerArrival::look(const MarkerLook& look){
    const std::optional<int>& distance = look.distance;
    bool taken = false;
    if (distance && *distance > 0 && (m_double_digits || *distance >= MARKER_PROMPT_NEAR)){
        m_far_digits = m_far_digits || *distance > MARKER_NEAR;
        taken = m_distance.read(*distance);
        m_double_digits = m_double_digits || (taken && *distance >= MARKER_PROMPT_NEAR);
    }
    const std::optional<int> at = m_distance.position();
    const bool close_or_marker_gone = !look.marker_shows || !at || *at < MARKER_PROMPT_NEAR;
    m_arrived = look.prompt_or_dialog && near() && close_or_marker_gone;
    return taken;
}

bool MarkerArrival::near() const{
    const std::optional<int> at = m_distance.position();
    if (at){
        return *at <= MARKER_NEAR;
    }
    return m_proof == MarkerProof::NO_FAR_READ && !m_far_digits;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
