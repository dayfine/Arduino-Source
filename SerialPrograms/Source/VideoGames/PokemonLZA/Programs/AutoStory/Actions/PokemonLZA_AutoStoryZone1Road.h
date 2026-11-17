/*  PokemonLZA AutoStory - Wild Zone 1 road
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free plan of 03.08's walks along Wild Zone 1's road: the
 *  catch spots the patrol goes between, the road stops back to the far
 *  gate, and where the checkpoint starts in the zone.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1Road_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1Road_H

#include <chrono>
#include <optional>
#include <span>
#include <vector>
#include "PokemonLZA_AutoStoryMarkerDistance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Wild Zone 1's two catch spots on its road, as distances from the
//  objective marker (the zone's far gate), the user's pick (2026-11-13):
//  100, the plaza by the green advertising column (Scatterbug on the
//  road), and 60, just before the far gate (Mareep in the grass right of
//  the road and on it). The patrol goes 100, 60, 100, 60, ...: walking to
//  100 and back makes the Pokemon at 60 come back when none showed there.
inline constexpr int CATCH_SPOTS[] = {100, 60};

//  The left stick held right (facing the far gate) that takes the player
//  from where the walk to a catch spot ends to the middle of the road: the
//  walk ended on the left sidewalk or grass at both spots, by the stall at
//  60 and the advertising column at 100 (console B, 2026-11-13), and the
//  user caught from mid-road. The way back is the same time left. A guess
//  from the shots (the road some 5 m wide there), NEEDS CONSOLE.
inline constexpr std::chrono::milliseconds ROAD_MIDDLE_STEP{1000};

//  How far short of a road stop still counts as back on the road there:
//  the walk back to 60 from nearer the gate stopped at 58 once (console B,
//  2026-11-14), and the gate walk from there is steered by the gate.
inline constexpr int ROAD_STOP_SLACK = 3;

//  `visits` stops going back and forth over `spots`: {100, 60, 100, ...}.
std::vector<int> alternating_stops(std::span<const int> spots, size_t visits);

//  The stops on the road to walk through, in order, from `distance` from
//  the far gate, before the last walk to the gate's "A Leave": the spots
//  of `spots` nearer the gate than `distance`, furthest first. At or past
//  the spot nearest the gate, that spot: back onto the road first, as a
//  walk straight at the gate from off the road ran into a building west
//  of the zone (console B, 2026-11-13, last read 49). With no distance
//  read, every spot.
std::vector<int> road_stops_to_gate(std::span<const int> spots, std::optional<int> distance);


//  What the walk between two road stops does after a look at the
//  objective marker (RoadWalkReads).
enum class RoadLook{
    STEP,               //  a step on: the read taken short of the stop, dropped, or none
    SIDESTEP_RIGHT,     //  held up: a sidestep (RoadWalkReads::sidesteps()), then a step on
    SIDESTEP_LEFT,
    ARRIVED,            //  a read taken at the stop or past it
};

//  The walk between the patrol's road stops, read by read, as
//  plza/autostory's walk_to_objective() took the marker's distance (03.08
//  passed about 11 times on console B, 2026-11-14). It steps on after
//  every look, a read taken or not: only arriving needs a read. The lean
//  walker stood on looks with no read taken, nudged the camera and
//  sidestepped, and ended blocked: its walks toward 60 ended at no read,
//  74 and 92, sideways off the road (console B, 2026-11-16 16:17).
class RoadWalkReads{
public:
    //  A walk `direction` to `stop_at`. `floor`: a read below it is taken
    //  only near the last read taken: by the green column at 101 to 105
    //  the digits read "1", "5", "21" and "41" (console B, 2026-11-16).
    RoadWalkReads(MarkerDirection direction, int stop_at, std::optional<int> floor)
        : m_direction(direction)
        , m_stop_at(stop_at)
        , m_floor(floor)
    {}

    //  A look with `distance` read (none: no digits). The first read is
    //  taken at once; after it, a read within 12 of the last one taken (a
    //  0.6 s step covers about 3), or a third in a row that agree with
    //  each other far from it (110 read as "1110", console B 2026-11-13).
    //  Toward the objective, 4 reads taken with none nearer than the best
    //  is a walk held up (the Research Lab's fence, console B 2026-11-10):
    //  a sidestep right, then left, 4 at most.
    RoadLook next(std::optional<int> distance);

    //  The last read taken, if any.
    std::optional<int> position() const{
        return m_last;
    }

    //  The held-up sidesteps so far; the n-th is held n times 1.5 s.
    size_t sidesteps() const{
        return m_sidesteps;
    }

private:
    bool take(int distance);
    bool reached() const;
    RoadLook held_up(int distance);

    const MarkerDirection m_direction;
    const int m_stop_at;
    const std::optional<int> m_floor;
    std::optional<int> m_last;
    std::optional<int> m_far_read;      //  the last read far from m_last
    size_t m_far_reads = 0;             //  reads in a row far from m_last that agree with each other
    std::optional<int> m_best;          //  the nearest read taken (toward)
    size_t m_unmoved_reads = 0;         //  reads taken in a row none nearer than m_best
    size_t m_sidesteps = 0;
};

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
