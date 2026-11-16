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

//  What a walk by the marker does after a look. With no read taken, it
//  stands and reads again, and every third look in a row with none it
//  tries a new view: the camera nudged a little left, then right, then a
//  short step to the side of the marker's line, left then right, each
//  further out, 4 at most; then it ends. Never a step toward or away
//  without a read taken: in run 4 of 03.08 (console B, 2026-11-16) an
//  NPC's speech bubble sat over the marker's digits, and re-centering put
//  the marker right back under it.
enum class MarkerMove{
    STEP,               //  a read taken, the stop not reached: a step on
    RECENTER,           //  no read taken: stand, the camera back onto the marker, read again
    REREAD,             //  no read taken after a nudge: stand, the camera kept where it is, read again
    NUDGE_CAMERA_LEFT,  //  the third look with none: the camera a little left, read again
    NUDGE_CAMERA_RIGHT,
    SIDESTEP_LEFT,      //  still none after both nudges: re-center, a short step left, read again
    SIDESTEP_RIGHT,
    BLOCKED,            //  still none after every sidestep: the walk ends
    ARRIVED,            //  a read taken at the stop (or, toward, at the floor)
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
    //  walk's direction (toward: at the floor too); else what the walk
    //  does for a new view (MarkerMove), by the looks in a row with none.
    MarkerMove next(std::optional<int> distance, int stop_at);

    //  The last read taken, if any.
    std::optional<int> position() const{
        return m_last;
    }

private:
    bool reached(int stop_at) const;
    MarkerMove unread_move();

    const MarkerDirection m_direction;
    const std::optional<int> m_floor;
    std::optional<int> m_last;
    std::vector<int> m_untaken;     //  the last three reads not taken, oldest first
    size_t m_unread_looks = 0;      //  looks in a row with no read taken
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
