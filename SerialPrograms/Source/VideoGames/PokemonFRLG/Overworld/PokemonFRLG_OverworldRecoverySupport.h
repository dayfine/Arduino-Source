/*  Overworld Recovery Support
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Internal route-log formatting and wandering-NPC map queries used by
 *  PokemonFRLG_OverworldRecovery.cpp.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldRecoverySupport_H
#define PokemonAutomation_PokemonFRLG_OverworldRecoverySupport_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Grid/Trace/Pokemon_RouteLog.h"
#include "PokemonFRLG_OverworldLegTrace.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRecovery.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The route's lines in the shared route log, plus one [Leg] trace per leg.
class RouteLogLines{
public:
    RouteLogLines(const RouteCalls& calls, const RoutePlanning* planning);
    void plan(const std::optional<MapPosition>& from, const std::vector<RouteStep>& steps) const;
    void leg(size_t index, const std::vector<RouteStep>& steps);
    void end(const RouteStep& step, const std::optional<MapPosition>& at, Pokemon::EndBasis basis, const std::string& why);
    void trainer_stopped(const RouteStep& step, const MapPosition& at);
    void battle_before();
    void found(const MapPosition& at, size_t holds) const;
    void done(const RouteStep& step, size_t number, size_t holds);

private:
    void log_end(const RouteStep& step, const std::optional<MapPosition>& at, Pokemon::EndBasis basis, const std::string& why) const;

    const RouteCalls& m_calls;
    std::string m_label;
    OverworldLegTrace m_trace;
};

//  Whether the tile one step `dir` ahead of `pos` lies inside a wander box.
bool ahead_has_wanderer(const std::vector<MapData>* maps, const MapPosition& pos, Direction dir);

//  Whether any stepped tile of `step` from `start` (or `hold_from`) lies
//  inside a wander box.
bool step_touches_wander_box(
    const std::vector<MapData>* maps,
    const RouteStep& step,
    const std::optional<MapPosition>& start,
    const std::vector<MapPosition>& hold_from
);

//  Whether an unmoved `step` could be blocked by a wandering NPC ahead of
//  `unmoved_starts`, `start`, or the first-hold candidate tiles.
bool step_blocked_by_wanderer(
    const std::vector<MapData>* maps,
    const RouteStep& step,
    const std::optional<MapPosition>& start,
    const std::vector<StopCandidate>& unmoved_starts,
    const std::vector<MapPosition>& hold_from,
    bool first_step,
    int across
);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
