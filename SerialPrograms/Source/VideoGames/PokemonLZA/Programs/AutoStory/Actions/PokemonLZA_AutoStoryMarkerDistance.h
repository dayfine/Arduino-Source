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
    explicit MarkerDistance(std::optional<int> floor)
        : m_floor(floor)
    {}

    //  Returns whether `distance` (above 0) is taken: never below the floor;
    //  else when within 12 of position(), or when two of the last three
    //  untaken reads agree with it. So the first read needs three that agree,
    //  and three that agree replace a wrong last one.
    bool read(int distance);

    std::optional<int> position() const{
        return m_last;
    }

private:
    const std::optional<int> m_floor;
    std::optional<int> m_last;
    std::vector<int> m_untaken;     //  oldest first
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
