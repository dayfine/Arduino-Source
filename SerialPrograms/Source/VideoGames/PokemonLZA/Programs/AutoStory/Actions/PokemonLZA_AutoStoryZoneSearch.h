/*  PokemonLZA AutoStory - Zone search
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How 03.08 looks for new species in Wild Zone 1 without walking by the
 *  objective marker's digits: stand, turn the camera in steps through a
 *  full turn with a ZL lock-on after each, catch a species not caught yet,
 *  and only when a full turn finds none take one short leg along the road,
 *  each leg out followed by one back. Six runs walking between distance
 *  stops (console B, 2026-11-16) left the zone or stood stuck: the digits
 *  read nothing for long stretches (speech bubbles, the marker at the
 *  screen's edge), and run 6 ended 4 of 5 with only species already
 *  caught locking on at its last stops.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZoneSearch_H
#define PokemonAutomation_PokemonLZA_AutoStoryZoneSearch_H

#include <cstddef>
#include <optional>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What one look of a scan found.
enum class ZoneLook{
    NO_PLATE,       //  ZL held, no name plate showed
    ALL_CAUGHT,     //  every plate the lock moved through shows the caught mark
    NOT_CAUGHT,     //  locked on, ZL still held, to a species not caught
    WILD_BATTLE,    //  a wild battle is on
};

//  What the search does after a look.
enum class ZoneMove{
    TURN,   //  the camera one step on, then a look
    CATCH,  //  a throw at the target locked on
    FIGHT,  //  the wild battle on is fought (weakened and caught, or fought off)
    WALK,   //  a full turn found nothing new: one leg (ZoneSearch::leg()), then a scan
    PAUSE,  //  every leg of a round walked: stand a while for spawns, then a scan
};

//  A leg's way, with the camera turned onto the objective marker: on
//  toward it, back away from it, or across its line.
enum class ZoneLeg{
    TOWARD,
    AWAY,
    LEFT,
    RIGHT,
};

//  Where the search stands, in legs from where it started: on toward the
//  objective (above 0) and right of its line (above 0).
struct ZoneOffset{
    int toward = 0;
    int right = 0;

    bool operator==(const ZoneOffset&) const = default;
};

//  The search, one look and one leg at a time. A scan is `steps_per_turn`
//  looks, the camera turned one step between two. A catch or a fight
//  starts the scan over. A scan with nothing new is a WALK; legs go out
//  from the start spot and back in turn (away, left, right, toward), and
//  once every way has been out and back, the next empty scan is a PAUSE
//  and the round starts over.
class ZoneSearch{
public:
    explicit ZoneSearch(size_t steps_per_turn)
        : m_steps_per_turn(steps_per_turn)
    {}

    //  What to do after a look at camera step step().
    ZoneMove look(ZoneLook look);

    //  The leg after a WALK: back toward the start spot when away from it,
    //  else the round's next way out. `distance` is an agreed read of the
    //  objective marker's distance, if any: at or below
    //  TOWARD_FLOOR_DISTANCE no leg goes toward the objective (the far
    //  gate is at ~55 and the city past it), and an away leg takes its
    //  place.
    ZoneLeg leg(std::optional<int> distance);

    //  The camera step of the scan, from 0.
    size_t step() const{
        return m_step;
    }

    ZoneOffset offset() const{
        return m_offset;
    }

private:
    const size_t m_steps_per_turn;
    size_t m_step = 0;
    size_t m_outs = 0;      //  legs out walked this round
    ZoneOffset m_offset;
};

//  An agreed distance read at or below this: no leg toward the objective.
constexpr int TOWARD_FLOOR_DISTANCE = 60;



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
