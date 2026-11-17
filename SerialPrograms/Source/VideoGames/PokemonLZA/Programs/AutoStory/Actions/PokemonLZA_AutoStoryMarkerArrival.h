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

//  One look on a walk to the objective's prompt.
struct MarkerLook{
    //  The marker's digits as read; none: no marker, or no digits.
    std::optional<int> distance;
    bool marker_shows = true;
    //  An A prompt or a dialog shows.
    bool prompt_or_dialog = false;
};

//  Where a walk to the objective's prompt is, one look at a time.
class MarkerArrival{
public:
    explicit MarkerArrival(MarkerProof proof)
        : m_proof(proof)
        , m_distance(MarkerDirection::TOWARD, std::nullopt)
    {}

    //  A look. Returns whether its distance is taken
    //  (MarkerDistance::read()).
    //  A read below 10 is taken only after one of 10 or more: junk OCR
    //  ("E5", "v1", "E6") by a street dialog agreed on 6 at the start of
    //  03.09's walk and passed it there (console B, 2026-11-16 18:47).
    bool look(const MarkerLook& look);

    //  Whether the last look showed an A prompt or a dialog with the walk
    //  near(), and, with the marker still up, with no read taken or the
    //  last below 10: a scene at the objective takes the marker away
    //  before its dialog (the lab gate's at 10 to 13, console B
    //  2026-11-14 and 2026-11-16 18:47), so a prompt with the marker up
    //  at 10 or more is something else on the way.
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
    bool m_double_digits = false;   //  a read of 10 or more taken
    bool m_arrived = false;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
