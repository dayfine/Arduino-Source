/*  PokemonLZA AutoStory - Marker distance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a walk by the objective marker is, by the marker's distance
 *  reads with misreads dropped.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerDistance_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerDistance_H

#include <optional>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Where a walk by the marker is: its last distance read taken.
class MarkerDistance{
public:
    //  `floor`: reads below it are never taken.
    explicit MarkerDistance(std::optional<int> floor)
        : m_floor(floor)
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

    //  The last read taken, if any.
    std::optional<int> position() const{
        return m_last;
    }

private:
    const std::optional<int> m_floor;
    std::optional<int> m_last;
    std::vector<int> m_untaken;     //  the last three reads not taken, oldest first
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
