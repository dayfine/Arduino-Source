/*  PokemonLZA AutoStory - Marker arrival
 *
 *  From: https://github.com/PokemonAutomation/
 *
  *  Whether a walk to the objective marker's prompt (walk_to_unread_marker())
  *  is there: an A prompt or a dialog counts only near the marker.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerArrival_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerArrival_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "PokemonLZA_AutoStoryMarkerDistance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What proves the walk is near the objective, so its prompt or dialog is
//  the objective's.
enum class MarkerProof{
    //  A distance read taken at 20 or less.
    NEAR_READ,
    //  For where the digits do not read: no read over 20 at all, or a read taken at 20 or less.
    NO_FAR_READ,
};

//  One look on a walk to the objective's prompt.
struct MarkerLook{
    //  nullopt: no marker, or no digits.
    std::optional<int> distance;
    bool marker_shows = true;
    bool prompt_or_dialog = false;
    WallClock at = {};
};

//  What came of a look's distance read.
struct MarkerRead{
    bool taken = false;
    //  Set when a read of 20 or less was dropped as nearer than the walk can
    //  have come since the last read taken: the time since that read.
    std::optional<Milliseconds> too_fast_after = std::nullopt;
};

//  Where a walk to the objective's prompt is, one look at a time.
class MarkerArrival{
public:
    explicit MarkerArrival(MarkerProof proof)
        : m_proof(proof)
        , m_distance(std::nullopt)
    {}

    //  A read below 10 is taken only after one of 10 or more, and one of 20 or
    //  less only if the walk can have come that near since the last read
    //  taken: junk OCR near a dialog can agree on a single digit.
    MarkerRead look(const MarkerLook& look);

    //  The last look showed a prompt or dialog while near(), and, with the
    //  marker still up, the last read is below 10 or none: a scene at the
    //  objective takes the marker away before its dialog.
    bool arrived() const{
        return m_arrived;
    }

    //  Whether the MarkerProof holds.
    bool near() const;

    std::optional<int> position() const{
        return m_distance.position();
    }

private:
    const MarkerProof m_proof;
    MarkerDistance m_distance;
    WallClock m_taken_at = {};
    bool m_far_digits = false;      //  digits over 20 read on some look
    bool m_double_digits = false;   //  a read of 10 or more taken
    bool m_arrived = false;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
