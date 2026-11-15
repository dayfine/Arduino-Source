/*  BDSP AutoStory - Leg Trace IO
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include <string_view>
#include "PokemonBDSP_AutoStory_LegTraceIO.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::LegEndKind;
using Pokemon::LegEndKnown;
using Pokemon::LegInterrupt;


Pokemon::LegTracer legs_to_log(RouteIO& io){
    return Pokemon::LegTracer([&io](const std::string& line){ io.note(line, RouteNote::LOG); });
}

void LegTraceRouteIO::walked(const WalkResult& result){
    m_tracer.walked({.chunks = result.moved_chunks, .walked_ms = result.walked.count(), .wild_battles = result.wild_battles});
}

WalkResult LegTraceRouteIO::walk_to_wall(const RouteWallWalk& walk){
    const WalkResult result = m_inner.walk_to_wall(walk);
    walked(result);
    return result;
}

WalkResult LegTraceRouteIO::walk_taps(Pokemon::Direction direction, size_t tiles, const std::string& label){
    const WalkResult result = m_inner.walk_taps(direction, tiles, label);
    m_tracer.walked({.chunks = result.moved_chunks, .walked_ms = result.walked.count(),
        .wild_battles = result.wild_battles, .tapped = result.moved_chunks});
    return result;
}

void LegTraceRouteIO::note(const std::string& text, RouteNote kind){
    if (kind == RouteNote::FAILURE){
        //  A failure does not return: the leg ends here, unknown.
        m_tracer.end_event();
        m_inner.note(text, kind);
        return;
    }
    m_inner.note(text, kind);
    read_route_line(text);
}

namespace{

//  The text between `after` and `before` in `text`, both searched from the
//  end; empty if either is missing.
std::string_view between_last(std::string_view text, std::string_view after, std::string_view before){
    const size_t end = text.rfind(before);
    if (end == std::string_view::npos){
        return {};
    }
    const size_t begin = text.rfind(after, end);
    if (begin == std::string_view::npos){
        return {};
    }
    return text.substr(begin + after.size(), end - begin - after.size());
}

} // namespace

//  The planned route walker's lines (PokemonBDSP_AutoStory_PlannedRoute.cpp,
//  Pokemon_RouteLog.h):
//      Leg 1/2: east 15 (to wall) to Route 202 (23, 25)
//      [Route] <label>: <leg>: ended at <tiles> (walked).
//      [Route] <label>: <leg>: ended at <tiles> (assumed: 1 wild battle(s) on the way).
//      [Route] <label>: <leg>: <what>, at <tiles>; planning again.
//      [Route] <label>: <leg>: through the warp at its end (<warp>); handing over.
//      [Route] <label>: <who> stopped us about <tiles>.
//      <label>: <leg>: the next step's scene started on the way; handing it over.
//      [Route] <label>: <leg>: blocked after 7 of 24 tiles; waiting for the way to clear.
//      <label>: <leg>: someone is in the way; talking to them.
void LegTraceRouteIO::read_route_line(const std::string& text){
    if (const std::optional<Pokemon::LegPlan> plan = Pokemon::parse_route_leg_line(text)){
        m_tracer.begin_leg(*plan);
        return;
    }
    const Pokemon::LegPlan* open = m_tracer.open_leg();
    if (open == nullptr){
        return;
    }
    const std::string_view line = text;
    if (line.ends_with(" (walked).") || line.find("(assumed: ") != std::string_view::npos){
        const bool assumed = line.find("(assumed: ") != std::string_view::npos;
        const std::string_view tiles = between_last(line, ": ended at ", assumed ? " (assumed: " : " (walked).");
        const LegEndKnown known = !assumed && open->end_kind == LegEndKind::WALL ? LegEndKnown::WALL : LegEndKnown::DEAD_RECKONED;
        m_tracer.end_leg(Pokemon::parse_route_tiles(tiles), known);
        return;
    }
    if (line.ends_with("; planning again.")){
        m_tracer.end_leg(Pokemon::parse_route_tiles(between_last(line, ", at ", "; planning again.")), LegEndKnown::SEEN, "replan");
        return;
    }
    if (line.ends_with("); handing over.") && line.find(": through the warp at its end (") != std::string_view::npos){
        m_tracer.end_leg(open->ends, LegEndKnown::FADE, "warp");
        return;
    }
    if (line.ends_with(": the next step's scene started on the way; handing it over.")){
        m_tracer.interrupted(LegInterrupt::SCENE);
        m_tracer.end_leg({}, LegEndKnown::DIALOGUE, "scene");
        return;
    }
    if (line.find(" stopped us about ") != std::string_view::npos){
        m_tracer.interrupted(LegInterrupt::TRAINER);
    }else if (line.ends_with("; waiting for the way to clear.") || line.ends_with(": someone is in the way; talking to them.")){
        m_tracer.interrupted(LegInterrupt::BLOCKED);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
