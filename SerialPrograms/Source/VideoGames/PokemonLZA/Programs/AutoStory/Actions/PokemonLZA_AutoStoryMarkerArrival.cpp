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

bool reads_agree(int a, int b){
    return a >= b - 12 && a <= b + 12;
}

constexpr size_t AGREEING_UNTAKEN = 2;
constexpr size_t UNTAKEN_KEPT = 3;

} // namespace


bool MarkerDistance::read(int distance){
    if (m_floor && distance < *m_floor){
        return false;
    }
    if (!m_last || !reads_agree(distance, *m_last)){
        size_t agreeing = 0;
        for (int untaken : m_untaken){
            if (reads_agree(distance, untaken)){
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


namespace{

//  The lab gate's scene takes the marker away at 13 or less; stray street
//  prompts came at 44 or more.
constexpr int MARKER_NEAR = 20;
constexpr int MARKER_PROMPT_NEAR = 10;
//  Meters a second: a passing walk measured 3.4, with bursts of 4 / 0.64 s.
constexpr int MARKER_MAX_SPEED = 5;
constexpr int MARKER_READ_JITTER = 2;

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
