/*  Overworld Leg Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "PokemonFRLG_OverworldLegTrace.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::LegEndKind;
using Pokemon::LegEndKnown;
using Pokemon::LegInterrupt;


namespace{

Pokemon::RouteTile route_tile(const MapPosition& position){
    return {position.map, position.point};
}

Pokemon::LegPlan plan_of(const RouteStep& step){
    Pokemon::LegPlan plan{.direction = step.direction, .tiles = (size_t)step.tiles, .end_kind = LegEndKind::OPEN, .ends = {route_tile(step.end)}};
    if (step.action == RouteAction::WALK && step.warps){
        plan.end_kind = LegEndKind::TILE;
    }else if (step.action == RouteAction::WALK && step.to_wall){
        plan.end_kind = LegEndKind::WALL;
    }
    return plan;
}

} // namespace


OverworldLegTrace::OverworldLegTrace(
    const std::string& checkpoint, const std::string& event,
    std::function<void(const std::string& line)> log
)
    : m_tracer(std::move(log))
{
    m_tracer.begin_event(checkpoint, event);
}

OverworldLegTrace::~OverworldLegTrace(){
    m_tracer.end_event();
}

void OverworldLegTrace::begin(const RouteStep& step){
    m_tracer.begin_leg(plan_of(step));
}

void OverworldLegTrace::done(const RouteStep& step){
    const bool warped = step.action == RouteAction::WALK && step.warps;
    m_tracer.end_leg({route_tile(step.end)}, warped ? LegEndKnown::FADE : LegEndKnown::DEAD_RECKONED);
}

void OverworldLegTrace::trainer_stopped(const MapPosition& at){
    m_tracer.interrupted(LegInterrupt::TRAINER);
    m_tracer.end_leg({route_tile(at)}, LegEndKnown::SEEN, "replan");
}

void OverworldLegTrace::unknown(const std::string& note){
    m_tracer.end_leg({}, LegEndKnown::UNKNOWN, note);
}

void OverworldLegTrace::held_to_warp(const RouteStep& step){
    m_tracer.end_leg({route_tile(step.end)}, LegEndKnown::FADE, "warp");
}

void OverworldLegTrace::battle_before(){
    m_tracer.interrupted(LegInterrupt::WILD_BATTLE);
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
