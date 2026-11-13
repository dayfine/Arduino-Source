/*  BDSP AutoStory - Leg Trace IO
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A RouteIO that feeds a LegTracer (Pokemon_LegTrace.h) the legs walked
 *  through it, on the console and the simulation alike, and passes
 *  everything on to the RouteIO it wraps. The planned route walker's legs
 *  are read from its own route lines: a leg starts at its "Leg n/m" line
 *  and ends at its "ended at", "planning again" or warp line; its walks add
 *  their moved chunks, held time and wild battles. The story's own leg
 *  steps (StoryWalkSteps) start and end their legs on the tracer; their
 *  walks through this RouteIO add up the same way.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_LegTraceIO_H
#define PokemonAutomation_PokemonBDSP_AutoStory_LegTraceIO_H

#include <functional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_LegTrace.h"
#include "PokemonBDSP_AutoStory_RouteIO.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Passes everything on to `inner`, and tells `tracer` of the legs walked.
//  The tracer writes its records where it was given, the RouteIO's log
//  (legs_to_log()).
class LegTraceRouteIO : public RouteIO{
public:
    LegTraceRouteIO(RouteIO& inner, Pokemon::LegTracer& tracer)
        : m_inner(inner)
        , m_tracer(tracer)
    {}

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override;
    virtual WalkResult walk_taps(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual void press(const RouteInput& input) override{
        m_inner.press(input);
    }
    virtual RouteSight sight_within(const RouteWatch& watch) override{
        return m_inner.sight_within(watch);
    }
    virtual bool holding_against_wall(Pokemon::Direction direction, const std::string& label) override{
        return m_inner.holding_against_wall(direction, label);
    }
    virtual void look(const std::function<void(const ImageViewRGB32& screen)>& at) override{
        m_inner.look(at);
    }
    virtual void note(const std::string& text, RouteNote kind) override;

private:
    void walked(const WalkResult& result);
    //  Reads a route walker's line for the leg it starts, ends or interrupts.
    void read_route_line(const std::string& text);

    RouteIO& m_inner;
    Pokemon::LegTracer& m_tracer;
};

//  A tracer whose records go to `io`'s log.
Pokemon::LegTracer legs_to_log(RouteIO& io);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
