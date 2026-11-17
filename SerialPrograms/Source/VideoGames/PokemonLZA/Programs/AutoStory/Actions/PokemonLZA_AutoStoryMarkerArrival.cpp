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
//  How fast a walk can come nearer the marker, in meters a second, and
//  how much more a read may drop for OCR's jitter. The walk that passed
//  03.09 in run 10 read 23 down to 12 in 3.2 s, 3.4 a second, with 16 to
//  12 in 0.64 s (console B, 2026-11-16 21:36:55 to 21:36:58).
constexpr int MARKER_MAX_SPEED = 5;
constexpr int MARKER_READ_JITTER = 2;

//  Whether a walk can come from `from` to `to` in `time`.
bool can_come_near(int from, int to, Milliseconds time){
    return (from - to - MARKER_READ_JITTER) * 1000 <= MARKER_MAX_SPEED * time.count();
}

} // namespace


MarkerRead MarkerArrival::look(const MarkerLook& look){
    const std::optional<int>& distance = look.distance;
    const std::optional<int> last = m_distance.position();
    MarkerRead read;
    if (distance && *distance > 0 && (m_double_digits || *distance >= MARKER_PROMPT_NEAR)){
        const Milliseconds since_taken = std::chrono::duration_cast<Milliseconds>(look.at - m_taken_at);
        if (last && *distance <= MARKER_NEAR && !can_come_near(*last, *distance, since_taken)){
            read.too_fast_after = since_taken;
        }else{
            m_far_digits = m_far_digits || *distance > MARKER_NEAR;
            read.taken = m_distance.read(*distance);
            m_double_digits = m_double_digits || (read.taken && *distance >= MARKER_PROMPT_NEAR);
        }
        if (read.taken){
            m_taken_at = look.at;
        }
    }
    const std::optional<int> at = m_distance.position();
    const bool close_or_marker_gone = !look.marker_shows || !at || *at < MARKER_PROMPT_NEAR;
    m_arrived = look.prompt_or_dialog && near() && close_or_marker_gone;
    return read;
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
