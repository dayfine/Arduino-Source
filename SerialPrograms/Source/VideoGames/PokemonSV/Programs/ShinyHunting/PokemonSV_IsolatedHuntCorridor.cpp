/*  Isolated Uphill Corridor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#include "PokemonSV_IsolatedHuntCorridor.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
bool budget_expired(IsolatedHuntResult& result, const IsolatedHuntLimits& limits,
    IsolatedHuntActions& actions, uint64_t power_started, uint64_t hunt_started
){
    const uint64_t now = actions.now_ms();
    if (now - power_started >= limits.power_duration_ms){
        result.stop = IsolatedHuntStop::POWER_EXPIRED;
        return true;
    }
    if (now - hunt_started >= limits.duration_ms){
        result.stop = IsolatedHuntStop::TIME_LIMIT;
        return true;
    }
    return false;
}
bool encounter(const IsolatedHuntRequest& request, IsolatedHuntActions& actions,
    IsolatedHuntResult& result, IsolatedHuntEvent event
){
    if (event == IsolatedHuntEvent::REFUSAL){
        result.refusals++;
        if (!actions.acquire_refused_target(request)){
            result.stop = IsolatedHuntStop::UNRESOLVED_REFUSAL;
            return true;
        }
    }
    result.encounters++;
    const auto battle = actions.handle_battle(request);
    if (battle == IsolatedHuntBattleResult::CONTINUE){ return false; }
    result.stop = battle == IsolatedHuntBattleResult::STOP ?
        IsolatedHuntStop::BATTLE_POLICY : IsolatedHuntStop::BATTLE_FAILED;
    return true;
}
bool interrupted(const IsolatedHuntRequest& request, IsolatedHuntActions& actions,
    IsolatedHuntResult& result
){
    if (actions.refusal_pending() || actions.battle_pending()){
        if (result.stop == IsolatedHuntStop::ROUND_LIMIT){ result.stop = IsolatedHuntStop::TRAVEL_FAILED; }
    }
    if (actions.refusal_pending()){
        encounter(request, actions, result, IsolatedHuntEvent::REFUSAL);
        return true;
    }
    if (actions.battle_pending()){
        encounter(request, actions, result, IsolatedHuntEvent::BATTLE);
        return true;
    }
    return false;
}
bool reset_ascent(const IsolatedHuntRequest& request, IsolatedHuntActions& actions,
    IsolatedHuntResult& result, const char* reason
){
    if (!actions.reset_ascent(request, reason)){
        result.stop = IsolatedHuntStop::TRAVEL_FAILED;
        interrupted(request, actions, result);
        return false;
    }
    result.refreshes++;
    return true;
}
struct AscentState{
    uint16_t leg = 0;
    uint8_t empty = 0;
    bool reset = false;
    const char* reason = "segment cap";
};
bool prepare_segment(const IsolatedHuntRequest& request, IsolatedHuntActions& actions,
    IsolatedHuntResult& result, AscentState& state
){
    const auto marker = state.reset ? IsolatedMarkerState::ARRIVED : actions.marker_state();
    if (interrupted(request, actions, result)){ return false; }
    if (marker == IsolatedMarkerState::ARRIVED){
        if (!reset_ascent(request, actions, result, state.reset ? state.reason : "marker arrival")){ return false; }
        state.leg = 0;
        state.reset = false;
        if (actions.marker_state() != IsolatedMarkerState::PRESENT){
            result.stop = IsolatedHuntStop::REANCHOR_FAILED;
            interrupted(request, actions, result);
            return false;
        }
    }else if (marker == IsolatedMarkerState::MISSING){
        result.stop = IsolatedHuntStop::MARKER_MISSING;
        return false;
    }
    return true;
}
bool align_ascent(const IsolatedHuntRequest& request, IsolatedHuntActions& actions, IsolatedHuntResult& result){
    if (!actions.realign_marker()){
        result.stop = IsolatedHuntStop::REANCHOR_FAILED;
        interrupted(request, actions, result);
        return false;
    }
    return true;
}
}
uint64_t isolated_stage_timeout_ms(uint64_t started, uint64_t deadline, uint64_t requested){
    const uint64_t remaining = deadline > started ? deadline - started : 0;
    return remaining < requested ? remaining : requested;
}
bool valid_isolated_ascent(const IsolatedAscentLimits& limits){
    return limits.segments_per_ascent > 0 && limits.segments_per_ascent <= 30 &&
        limits.segment_ms >= 100 && limits.segment_ms <= 2000 &&
        limits.clearing_wait_ms >= 2000 && limits.clearing_wait_ms <= 15000;
}
IsolatedHuntResult run_isolated_corridor(const IsolatedHuntRequest& request,
    const IsolatedHuntLimits& limits, IsolatedHuntActions& actions,
    uint64_t power_started, uint64_t hunt_started
){
    if (!valid_isolated_ascent(limits.ascent) || limits.max_rounds == 0 || limits.max_empty_rounds == 0){
        return {.stop = IsolatedHuntStop::INVALID_PLAN};
    }
    IsolatedHuntResult result{.stop = IsolatedHuntStop::ROUND_LIMIT};
    AscentState state;
    while (result.rounds < limits.max_rounds){
        if (interrupted(request, actions, result)){ return result; }
        if (budget_expired(result, limits, actions, power_started, hunt_started)){ return result; }
        if (!prepare_segment(request, actions, result, state)){ return result; }
        if (budget_expired(result, limits, actions, power_started, hunt_started)){ return result; }
        if (!align_ascent(request, actions, result)){ return result; }
        if (budget_expired(result, limits, actions, power_started, hunt_started)){ return result; }
        const auto round = actions.advance_ascent(request, limits.ascent);
        if (round.budget_exhausted){
            result.stop = power_started + limits.power_duration_ms < hunt_started + limits.duration_ms ?
                IsolatedHuntStop::POWER_EXPIRED : IsolatedHuntStop::TIME_LIMIT;
            return result;
        }
        result.rounds++;
        result.kills += round.kills;
        state.reset = ++state.leg >= limits.ascent.segments_per_ascent;
        state.reason = "segment cap";
        if (round.event != IsolatedHuntEvent::CLEAR){
            if (encounter(request, actions, result, round.event)){ return result; }
            state.reset = true;
            state.reason = "handled encounter";
            state.empty = 0;
            continue;
        }
        if (round.clearing_busy){ result.stop = IsolatedHuntStop::CLEARING_TIMEOUT; return result; }
        state.empty = round.kills == 0 ? state.empty + 1 : 0;
        if (state.empty >= limits.max_empty_rounds){ result.stop = IsolatedHuntStop::NO_PROGRESS; return result; }
    }
    return result;
}
}
}
}
