/*  PokemonLZA AutoStory - Marker distance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a walk by the objective marker is, by the marker's distance
 *  reads with misreads dropped, and whether it steps on. It steps only on
 *  a read taken: with none, it stands, re-centers the camera on the marker
 *  and reads again. On 03.08's walk to 100 (console B, 2026-11-16) the
 *  digits read 109, then nothing or garbage for 13 s while the walk kept
 *  stepping, and it came out at 37 in the city, through the far gate.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerDistance_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerDistance_H

#include <optional>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Which way a walk by the marker goes: on toward it (the distance drops)
//  or back away from it (the distance grows).
enum class MarkerDirection{
    TOWARD,
    BACK,
};

//  What a walk by the marker does after a look.
enum class MarkerMove{
    STEP,       //  a read taken, the stop not reached: a step on
    RECENTER,   //  no read taken on this look: stand, re-center, read again
    ARRIVED,    //  a read taken at the stop (or, toward, at the floor)
};

//  Where a walk by the marker is: its last distance read taken.
class MarkerDistance{
public:
    //  `floor`: reads below it are never taken, and a walk toward the
    //  objective ends at it.
    MarkerDistance(MarkerDirection direction, std::optional<int> floor)
        : m_direction(direction)
        , m_floor(floor)
    {}

    //  A distance read (above 0) on a look. Below the floor it is not
    //  taken: the marker half behind Taunie's rocks read 1, 1 and 4 at
    //  the start of 03.08 while it showed 120 (runs 2 and 3, console B
    //  2026-11-16). Else it is taken when within 12 of position(), or when
    //  two of the last three reads not taken agree with it (each within
    //  12): so the first read is taken only once three agree, and three
    //  that agree replace a wrong last one (110 read as "1110" on console
    //  B, 2026-11-13). Returns whether the read is taken.
    bool read(int distance);

    //  A look with `distance` read (none: no digits): STEP or ARRIVED
    //  only when the read is taken, ARRIVED at `stop_at` or past it in the
    //  walk's direction (toward: at the floor too); else RECENTER.
    MarkerMove next(std::optional<int> distance, int stop_at);

    //  The last read taken, if any.
    std::optional<int> position() const{
        return m_last;
    }

private:
    bool reached(int stop_at) const;

    const MarkerDirection m_direction;
    const std::optional<int> m_floor;
    std::optional<int> m_last;
    std::vector<int> m_untaken;     //  the last three reads not taken, oldest first
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
