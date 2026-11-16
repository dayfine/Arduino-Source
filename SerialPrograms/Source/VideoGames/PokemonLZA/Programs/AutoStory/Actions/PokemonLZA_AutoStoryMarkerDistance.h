/*  PokemonLZA AutoStory - Marker distance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a walk by the objective marker is, by the marker's distance
 *  reads: misreads dropped, and steps taken with no read counted on from
 *  the last read. The marker's digits do not read for stretches (03.08 on
 *  console B, 2026-11-16: nothing taken for 13 s between 109 and 42), and
 *  a walk to a stop that went on blind walked past it and out of Wild
 *  Zone 1 into the city.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerDistance_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerDistance_H

#include <optional>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Which way a walk by the marker goes: on toward it (the distance drops)
//  or back away from it (the distance grows).
enum class MarkerDirection{
    TOWARD,
    BACK,
};

//  Where a walk by the marker is: its distance reads, misreads dropped,
//  and the 0.6 s steps taken since the last read taken.
class MarkerDistance{
public:
    explicit MarkerDistance(MarkerDirection direction)
        : m_direction(direction)
    {}

    //  A distance read (above 0) on a look. A read more than 12 from the
    //  last one taken is a misread (a step covers about 3; 36 read as "3",
    //  110 as "1110" on console B, 2026-11-13), unless three in a row
    //  agree: then the last one taken was the wrong one. Returns whether
    //  the read is taken.
    bool read(int distance);

    //  A step on, in the walk's direction: counted until the next read is
    //  taken.
    void stepped();

    //  The last read taken, if any.
    std::optional<int> last_read() const{
        return m_last;
    }

    //  Where the walk is: the last read taken, moved on by 3 (what a step
    //  covers) for each step since it. None before a read.
    std::optional<int> position() const;

    //  Steps since the last read taken.
    int steps_unread() const{
        return m_steps_unread;
    }

    //  Whether position() is at `stop_at` or past it in the walk's
    //  direction. Not before a read.
    bool reached(int stop_at) const;

private:
    const MarkerDirection m_direction;
    std::optional<int> m_last;
    std::optional<int> m_rejected_read;
    int m_rejected = 0;
    int m_steps_unread = 0;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
