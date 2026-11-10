/*  Leg Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One structured record per walked leg, shared by the games' walkers and
 *  their simulations, so a console run's route can be compared leg by leg
 *  with the simulation's (devtools/maps/leg_trace.py). Each leg is one log
 *  line beside the free-text route lines:
 *
 *      [Leg] {"checkpoint":"00.04","event":"to_the_signs_row","leg":1,
 *             "dir":"north","tiles":1,"end_kind":"tile","moved":1,...}
 *
 *  A record holds what was planned (direction, tiles, how the leg ends and
 *  the tiles it may end on), what was measured (taps that moved, the
 *  walk's moved chunks and held time), where the leg ended and how that is
 *  known, and what interrupted it.
 *
 */

#ifndef PokemonAutomation_Pokemon_LegTrace_H
#define PokemonAutomation_Pokemon_LegTrace_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon_Grid.h"
#include "Pokemon_RouteLog.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Prefix tag of a leg record's log line.
inline constexpr const char* LEG_TRACE_TAG = "[Leg] ";

//  How a leg is planned to end.
enum class LegEndKind{
    OPEN,   //  timed or tapped into the open
    WALL,   //  held until a wall stops it
    TILE,   //  counted taps onto an exact tile
};
//  "open", "wall", "tile".
const char* leg_end_kind_name(LegEndKind kind);

//  How the end of a leg is known.
enum class LegEndKnown{
    WALL,           //  a wall stopped the walk
    FADE,           //  a warp's fade or banner ended it
    DIALOGUE,       //  a dialogue stopped it
    COUNTED,        //  taps that moved, counted against the map
    DEAD_RECKONED,  //  taken to have ended as planned
    SEEN,           //  stopped short, where the walker found itself
    UNKNOWN,        //  not known: the leg was cut off
};
//  "wall", "fade", "dialogue", "counted", "dead_reckoned", "seen", "unknown".
const char* leg_end_known_name(LegEndKnown known);

//  What stopped or delayed a leg on the way.
enum class LegInterrupt{
    WILD_BATTLE,
    TRAINER,
    SCENE,
    DIALOGUE,
    BLOCKED,    //  someone the map lacks stood in the way
};
//  "wild_battle", "trainer", "scene", "dialogue", "blocked".
const char* leg_interrupt_name(LegInterrupt interrupt);

//  What a leg is planned to do.
struct LegPlan{
    Direction direction = Direction::NORTH;
    size_t tiles = 0;
    LegEndKind end_kind = LegEndKind::OPEN;
    //  Every tile the leg may end on; empty where the plan does not say.
    std::vector<RouteTile> ends;
};

//  One walked leg.
struct LegRecord{
    std::string checkpoint;
    std::string event;
    //  1-based, counted through the event, replans included.
    size_t leg = 0;
    LegPlan plan;
    //  The taps that moved, where the leg counts them.
    std::optional<size_t> moved;
    //  What the walks of the leg report: chunks of screen movement, and the
    //  time the direction was held.
    size_t chunks = 0;
    int64_t walked_ms = 0;
    //  The tiles the leg ended on, as the walker knows them.
    std::vector<RouteTile> end;
    LegEndKnown known = LegEndKnown::UNKNOWN;
    std::vector<LegInterrupt> interrupts;
    //  Why the leg ended other than as planned ("replan", "warp"), or "".
    std::string note;
};

//  The record as one line of compact JSON, keys sorted.
std::string leg_trace_json(const LegRecord& record);
//  LEG_TRACE_TAG and the record's JSON.
std::string leg_trace_line(const LegRecord& record);


//  The route log's text read back: "Route 202 (23, 25), Route 202 (30, 26)".
//  Nothing for text that is not a list of tiles.
std::vector<RouteTile> parse_route_tiles(std::string_view text);
//  A route_leg_line() read back: "Leg 1/2: east 15 (to wall) to <tiles>".
//  A warp leg ends at its tiles, as a tile leg.
std::optional<LegPlan> parse_route_leg_line(std::string_view text);


//  What one walk of a leg reports.
struct LegWalk{
    //  Chunks of screen movement, and the time the direction was held.
    size_t chunks = 0;
    int64_t walked_ms = 0;
    size_t wild_battles = 0;
    //  The taps that moved, for a walk of taps.
    std::optional<size_t> tapped;
};

//  Builds a walker's leg records from what it does, and hands each, as its
//  log line, to `emit` once the leg ends. A leg still open when the next
//  starts, or when its event ends, is emitted with its end unknown.
class LegTracer{
public:
    explicit LegTracer(std::function<void(const std::string& line)> emit)
        : m_emit(std::move(emit))
    {}

    //  The legs from here on are `event`'s of `checkpoint`.
    void begin_event(const std::string& checkpoint, const std::string& event);
    //  Emits the leg still open, if one is.
    void end_event();

    //  A leg starts.
    void begin_leg(LegPlan plan);
    //  The open leg's plan; null if no leg is open.
    const LegPlan* open_leg() const{
        return m_leg ? &m_leg->plan : nullptr;
    }
    //  A walk of the open leg ended. A leg can take several (a walk on to
    //  the wall after a battle); they add up.
    void walked(const LegWalk& walk);
    void interrupted(LegInterrupt interrupt);
    //  The open leg ended on `end`, known by `known`; emits it.
    void end_leg(std::vector<RouteTile> end, LegEndKnown known, std::string note = "");

private:
    std::function<void(const std::string& line)> m_emit;
    std::string m_checkpoint;
    std::string m_event;
    size_t m_legs = 0;
    std::optional<LegRecord> m_leg;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
