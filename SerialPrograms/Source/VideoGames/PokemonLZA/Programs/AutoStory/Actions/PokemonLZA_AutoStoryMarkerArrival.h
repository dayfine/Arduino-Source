/*  PokemonLZA AutoStory - Marker arrival
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure distance filtering and arrival state machine for objective-marker walks.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerArrival_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerArrival_H

#include <optional>
#include <vector>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Filters noisy objective-marker distance readings against an optional lower `floor`.
class MarkerDistance{
public:
    explicit MarkerDistance(std::optional<int> floor)
        : m_floor(floor)
    {}

    //  Considers `distance` (> 0); returns true if accepted as the updated position().
    bool read(int distance);

    std::optional<int> position() const{
        return m_last;
    }

private:
    const std::optional<int> m_floor;
    std::optional<int> m_last;
    std::vector<int> m_untaken;
};

//  Evidence level required to treat an interact prompt or dialog as objective arrival.
enum class MarkerProof{
    NEAR_READ,
    NO_FAR_READ,
};

//  Single frame observation fed to MarkerArrival::look().
struct MarkerLook{
    std::optional<int> distance;
    bool marker_shows = true;
    bool prompt_or_dialog = false;
    WallClock at = {};
};

//  Outcome of evaluating one MarkerLook's distance reading.
struct MarkerRead{
    bool taken = false;
    std::optional<Milliseconds> too_fast_after = std::nullopt;
};

//  Tracks marker distance and determines when the player has arrived at the objective.
class MarkerArrival{
public:
    explicit MarkerArrival(MarkerProof proof)
        : m_proof(proof)
        , m_distance(std::nullopt)
    {}

    //  Processes one observation and updates near() / arrived() state.
    MarkerRead look(const MarkerLook& look);

    //  True once a prompt or dialog was observed while near() under arrival rules.
    bool arrived() const{
        return m_arrived;
    }

    //  True when the configured MarkerProof currently holds.
    bool near() const;

    std::optional<int> position() const{
        return m_distance.position();
    }

private:
    const MarkerProof m_proof;
    MarkerDistance m_distance;
    WallClock m_taken_at = {};
    bool m_far_digits = false;
    bool m_double_digits = false;
    bool m_arrived = false;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
