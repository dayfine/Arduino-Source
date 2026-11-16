/*  PokemonLZA AutoStory - Zone start
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How 03.08 gets from its start save at Taunie's rocks to the road's
 *  spot at 100, where ZoneSearch looks for new species: a step west, then
 *  the marker walk (the user, 2026-11-16). Run 9 on console B (2026-11-16)
 *  searched at the rocks: two full turns with no name plate and no marker
 *  read in 8 looks; runs 5 and 6 reached 100 by the marker walk and threw
 *  there.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZoneStart_H
#define PokemonAutomation_PokemonLZA_AutoStoryZoneStart_H

#include <cstddef>
#include <optional>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The search spot, as the objective marker's distance: the plaza on the
//  road by the green advertising column (the user's pick, 2026-11-13),
//  where runs 5 and 6 threw (console B, 2026-11-16).
constexpr int ZONE_SPOT_DISTANCE = 100;

//  Whether an agreed distance read is at the search spot.
bool at_zone_spot(std::optional<int> distance);


//  What the way to the search spot does next.
enum class ZoneStartMove{
    LEAVE_WEST,         //  a step west by the minimap, then a distance read
    WALK_TO_SPOT,       //  the marker walk to ZONE_SPOT_DISTANCE
    SEARCH,             //  at the spot: the search (ZoneSearch)
    ROCKS_NOT_LEFT,     //  stop: no agreed read after every step west
    SPOT_NOT_REACHED,   //  stop: the marker walk did not end at the spot
};

//  03.08's way from its start save at Taunie's rocks to the search spot,
//  read by read (the user, 2026-11-16): the rocks hide the objective
//  marker (run 9, console B 2026-11-16: no read in 8 looks), so a step
//  west onto the road first, more while no read agrees, then the marker
//  walk to the spot. A start already at the spot searches there. A stop
//  stays a stop.
class ZoneStart{
public:
    explicit ZoneStart(size_t max_west_steps)
        : m_max_west_steps(max_west_steps)
    {}

    //  The move after `distance`, the agreed read (none: no read) at the
    //  start, after a step west, or at the end of the marker walk.
    ZoneStartMove next(std::optional<int> distance);

private:
    const size_t m_max_west_steps;
    size_t m_west_steps = 0;
    std::optional<ZoneStartMove> m_last;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
