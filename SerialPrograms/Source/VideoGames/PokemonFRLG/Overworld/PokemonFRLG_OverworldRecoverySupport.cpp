/*  Overworld Recovery Support
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon/Navigation/Pokemon_WanderRules.h"
#include "PokemonFRLG_OverworldHeldInPlace.h"
#include "PokemonFRLG_OverworldRecoverySupport.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::EndBasis;
using Pokemon::ROUTE_LOG_TAG;

const MapData* map_named(const std::vector<MapData>& maps, const std::string& name){
    for (const MapData& map : maps){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

std::vector<Pokemon::RouteTile> route_tiles(const std::optional<MapPosition>& position){
    if (!position){
        return {};
    }
    return {{position->map, position->point}};
}

Pokemon::RouteLogLeg route_log_leg(const RouteStep& step){
    Pokemon::RouteLogLeg leg{.action = Pokemon::RouteLegAction::WALK, .direction = step.direction, .tiles = (size_t)step.tiles, .end = Pokemon::RouteLegEnd::OPEN};
    if (step.action == RouteAction::SURF){
        leg.action = Pokemon::RouteLegAction::SURF;
    }else if (step.action == RouteAction::FACE){
        leg.action = Pokemon::RouteLegAction::FACE;
    }else if (step.action == RouteAction::OPEN_GATE){
        leg.action = Pokemon::RouteLegAction::OPEN_GATE;
    }else if (step.action == RouteAction::PRESS_SWITCH){
        leg.action = Pokemon::RouteLegAction::PRESS_SWITCH;
    }else if (step.warps){
        leg.end = Pokemon::RouteLegEnd::WARP;
    }else if (step.to_wall){
        leg.end = Pokemon::RouteLegEnd::WALL;
    }
    return leg;
}

bool leg_tiles_touch_wander_box(const std::vector<MapData>& maps, const std::vector<MapPosition>& tiles){
    return Pokemon::leg_touches_wander_box(tiles, [&](const MapPosition& pos){
        return map_named(maps, pos.map);
    });
}

} // namespace


RouteLogLines::RouteLogLines(const RouteCalls& calls, const RoutePlanning* planning)
    : m_calls(calls)
    , m_label(planning != nullptr ? planning->label : "Route")
    , m_trace(
        planning != nullptr ? planning->checkpoint : "", m_label,
        [&calls](const std::string& line){ calls.log(line); }
    )
{}

void RouteLogLines::plan(const std::optional<MapPosition>& from, const std::vector<RouteStep>& steps) const{
    std::vector<Pokemon::PlannedRouteLeg> legs;
    for (const RouteStep& step : steps){
        legs.push_back({.leg = route_log_leg(step), .after = route_tiles(step.end)});
    }
    m_calls.log(std::format("{}{}", ROUTE_LOG_TAG, Pokemon::route_plan_line(m_label, route_tiles(from), legs)));
}

void RouteLogLines::leg(size_t index, const std::vector<RouteStep>& steps){
    const RouteStep& step = steps[index];
    m_calls.log(std::format("{}{}", ROUTE_LOG_TAG, Pokemon::route_leg_line(index + 1, steps.size(), {.leg = route_log_leg(step), .after = route_tiles(step.end)})));
    m_trace.begin(step);
}

void RouteLogLines::end(const RouteStep& step, const std::optional<MapPosition>& at, EndBasis basis, const std::string& why){
    log_end(step, at, basis, why);
    if (basis == EndBasis::ASSUMED){
        m_trace.held_to_warp(step);
    }else{
        m_trace.unknown(why == "stopped part-way" ? "replan" : "again");
    }
}

void RouteLogLines::trainer_stopped(const RouteStep& step, const MapPosition& at){
    m_calls.log(std::format("{}{}", ROUTE_LOG_TAG, Pokemon::route_replan_line(m_label, route_log_leg(step), "stopped by a trainer who saw the player", route_tiles(at))));
    m_trace.trainer_stopped(at);
}

void RouteLogLines::battle_before(){
    m_trace.battle_before();
}

void RouteLogLines::found(const MapPosition& at, size_t holds) const{
    m_calls.log(std::format("{}{}", ROUTE_LOG_TAG, Pokemon::route_found_line(m_label, route_tiles(at), holds)));
}

void RouteLogLines::done(const RouteStep& step, size_t number, size_t holds){
    if (number < holds){
        log_end(step, std::nullopt, EndBasis::UNKNOWN, "a hold from an unknown tile");
        m_trace.unknown("hold");
        return;
    }
    log_end(step, step.end, EndBasis::WALKED, "");
    m_trace.done(step);
    if (number == holds){
        found(step.end, holds);
    }
}

void RouteLogLines::log_end(const RouteStep& step, const std::optional<MapPosition>& at, EndBasis basis, const std::string& why) const{
    const auto leg = route_log_leg(step);
    m_calls.log(std::format("{}{}", ROUTE_LOG_TAG, Pokemon::route_leg_end_line(m_label, leg, route_tiles(at), basis, why)));
    if (m_calls.leg_ended){
        const std::string path = m_calls.leg_ended(to_string(leg));
        if (!path.empty()){
            m_calls.log(std::format("{}{}: leg screenshot {}", ROUTE_LOG_TAG, m_label, path));
        }
    }
}

bool ahead_has_wanderer(const std::vector<MapData>* maps, const MapPosition& pos, Direction dir){
    if (maps == nullptr){
        return false;
    }
    const MapData* map = map_named(*maps, pos.map);
    if (map == nullptr){
        return false;
    }
    const TilePos ahead = pos.point.next(dir);
    return in_bounds(*map, ahead) && wander_object_at(*map, ahead) != nullptr;
}

bool step_touches_wander_box(
    const std::vector<MapData>* maps,
    const RouteStep& step,
    const std::optional<MapPosition>& start,
    const std::vector<MapPosition>& hold_from
){
    if (maps == nullptr){
        return false;
    }
    if (start){
        return leg_tiles_touch_wander_box(*maps, leg_tiles(*maps, *start, step));
    }
    for (const MapPosition& from : hold_from){
        if (leg_tiles_touch_wander_box(*maps, leg_tiles(*maps, from, step))){
            return true;
        }
    }
    return false;
}

bool step_blocked_by_wanderer(
    const std::vector<MapData>* maps,
    const RouteStep& step,
    const std::optional<MapPosition>& start,
    const std::vector<StopCandidate>& unmoved_starts,
    const std::vector<MapPosition>& hold_from,
    bool first_step,
    int across
){
    if (maps == nullptr){
        return false;
    }
    if (!unmoved_starts.empty()){
        for (const StopCandidate& candidate : unmoved_starts){
            if (ahead_has_wanderer(maps, candidate.player, step.direction)){
                return true;
            }
        }
        return false;
    }
    if (start){
        return ahead_has_wanderer(maps, *start, step.direction);
    }
    if (!first_step){
        return false;
    }
    for (const StopCandidate& candidate : before_people(*maps, hold_from, step, across)){
        if (ahead_has_wanderer(maps, candidate.player, step.direction)){
            return true;
        }
    }
    return false;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
