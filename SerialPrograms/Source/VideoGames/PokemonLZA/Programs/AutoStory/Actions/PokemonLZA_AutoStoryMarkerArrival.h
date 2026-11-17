/*  PokemonLZA AutoStory - Marker arrival
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether a walk to the objective marker's prompt (walk_to_marker()) is
 *  there: an A prompt or a dialog counts only near the marker. Any prompt
 *  used to: 03.09's walk from Wild Zone 1's far gate read no distance,
 *  stepped on blind and passed on a prompt on the street 4 s in, so 03.10
 *  started from the street (console B, 2026-11-16 17:34).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerArrival_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerArrival_H

#include <optional>
#include "PokemonLZA_AutoStoryMarkerDistance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What proves the walk is near the objective, so its prompt or dialog is
//  the objective's.
enum class MarkerProof{
    //  A distance read taken (MarkerDistance) at 20 or less: the walk that
    //  passed 03.09 read 37 down to 13 before the lab gate's scene took
    //  the marker away (console B, 2026-11-14).
    NEAR_READ,
    //  Where the marker's digits do not read (the lab's door up its
    //  steps: four looks, no digits, console B 2026-11-14): no read over
    //  20 at all, or a read taken at 20 or less.
    NO_FAR_READ,
};

//  Where a walk to the objective's prompt is, one look at a time.
class MarkerArrival{
public:
    explicit MarkerArrival(MarkerProof proof)
        : m_proof(proof)
        , m_distance(MarkerDirection::TOWARD, std::nullopt)
    {}

    //  A look: the marker's digits read (none: no marker, or no digits),
    //  and whether an A prompt or a dialog shows. Returns whether the read
    //  is taken (MarkerDistance::read()).
    bool look(std::optional<int> distance, bool prompt_or_dialog);

    //  Whether the last look showed an A prompt or a dialog with the walk
    //  near().
    bool arrived() const{
        return m_arrived;
    }

    //  Whether the proof holds: the last read taken is 20 or less; with
    //  none taken, for NO_FAR_READ, no digits over 20 read since the start.
    bool near() const;

    //  The last read taken, if any.
    std::optional<int> position() const{
        return m_distance.position();
    }

private:
    const MarkerProof m_proof;
    MarkerDistance m_distance;
    bool m_far_digits = false;  //  digits over 20 read on some look
    bool m_arrived = false;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
