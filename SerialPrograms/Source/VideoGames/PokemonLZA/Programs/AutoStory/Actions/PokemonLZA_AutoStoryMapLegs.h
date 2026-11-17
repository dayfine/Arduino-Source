/*  PokemonLZA AutoStory - Map legs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  03.08's walks in Wild Zone 1 as legs between waypoints on the full map
 *  (read_map_pin(), MapWaypoint): each leg turns to the bearing toward its waypoint by
 *  the minimap's arrow, holds forward for the distance's time, then reads
 *  the pin on the map again and corrects. The marker-distance walks ended
 *  off the road (console B, 2026-11-16); the map shows where they ended.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMapLegs_H
#define PokemonAutomation_PokemonLZA_AutoStoryMapLegs_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMapPinReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What a leg does after a pin read.
enum class MapLegStep{
    ARRIVED,        //  at the waypoint
    WALK,           //  turn to `heading`, hold forward `hold`, read again
    READ_AGAIN,     //  no pin read: open the map again
    OFF_TARGET,     //  stop: still off after the leg and its corrections
    PIN_NOT_READ,   //  stop: no pin on two reads in a row
};

//  A leg's next move after a pin read (MapLeg::next()).
struct MapLegMove{
    MapLegStep step = MapLegStep::ARRIVED;
    //  WALK: the minimap heading toward the waypoint (0 up, 90 right).
    double heading = 0;
    Milliseconds hold = Milliseconds(0);
    //  WALK: 0 the leg itself, 1 and 2 its corrections.
    size_t correction = 0;
};

//  One leg to `target`, pin read by pin read: a read within 6 map pixels
//  (~5 marker units) is there; so is one within `stop_radius` that is the
//  leg's first read or lies past `target` on the last walk's way (no
//  turning back at a catch spot). The first read off it walks the leg,
//  each later one a correction, 2 at most; then a read still off stops
//  it. One missing read gets a second look.
class MapLeg{
public:
    MapLeg(MapPoint target, double stop_radius)
        : m_target(target)
        , m_stop_radius(stop_radius)
    {}

    MapLegMove next(std::optional<MapPoint> pin);

private:
    bool in_stop_area(MapPoint pin, double distance) const;

private:
    const MapPoint m_target;
    const double m_stop_radius;
    size_t m_walks = 0;
    bool m_missed = false;
    //  Where the last walk started; none: no walk yet.
    std::optional<MapPoint> m_walked_from;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
