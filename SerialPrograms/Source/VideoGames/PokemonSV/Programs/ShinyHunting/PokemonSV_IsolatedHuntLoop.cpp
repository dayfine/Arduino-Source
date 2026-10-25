#include "PokemonSV_IsolatedHuntLoop.h"
#include "PokemonSV_IsolatedHuntCorridor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

bool handle_battle_policy(const IsolatedHuntRequest& request, IsolatedHuntActions& actions, IsolatedHuntResult& result){
    result.encounters++;
    switch (actions.handle_battle(request)){
    case IsolatedHuntBattleResult::STOP:
        result.stop = IsolatedHuntStop::BATTLE_POLICY;
        return true;
    case IsolatedHuntBattleResult::FAILED:
        result.stop = IsolatedHuntStop::BATTLE_FAILED;
        return true;
    case IsolatedHuntBattleResult::CONTINUE:
        return false;
    }
    result.stop = IsolatedHuntStop::BATTLE_FAILED;
    return true;
}

IsolatedHuntResult failed_stage(const IsolatedHuntRequest& request, IsolatedHuntActions& actions, IsolatedHuntStop reason){
    IsolatedHuntResult result{.stop = reason};
    if (actions.refusal_pending()){
        result.refusals++;
        if (!actions.acquire_refused_target(request)){
            result.stop = IsolatedHuntStop::UNRESOLVED_REFUSAL;
            return result;
        }
        handle_battle_policy(request, actions, result);
    }else if (actions.battle_pending()){
        handle_battle_policy(request, actions, result);
    }
    return result;
}

bool handle_encounter(const IsolatedHuntRequest& request, IsolatedHuntActions& actions, IsolatedHuntResult& result){
    if (handle_battle_policy(request, actions, result)){
        return true;
    }
    if (!actions.travel_to_anchor(request)){
        result.stop = IsolatedHuntStop::TRAVEL_FAILED;
        if (actions.battle_pending()){
            handle_battle_policy(request, actions, result);
        }
        return true;
    }
    return false;
}

bool handle_event(const IsolatedHuntRequest& request, IsolatedHuntActions& actions,
    IsolatedHuntResult& result, IsolatedHuntEvent event
){
    if (event == IsolatedHuntEvent::REFUSAL){
        result.refusals++;
        if (!actions.acquire_refused_target(request)){
            result.stop = IsolatedHuntStop::UNRESOLVED_REFUSAL;
            return true;
        }
    }
    return handle_encounter(request, actions, result);
}

bool handle_empty_round(const IsolatedHuntRequest& request, const IsolatedHuntLimits& limits,
    IsolatedHuntActions& actions, IsolatedHuntResult& result, uint8_t empty, bool allow_refresh
){
    if (empty >= limits.max_empty_rounds){
        result.stop = IsolatedHuntStop::NO_PROGRESS;
        return true;
    }
    if (empty > 0 && allow_refresh){
        if (!actions.refresh_spawns(request)){
            result.stop = IsolatedHuntStop::REFRESH_FAILED;
            if (actions.battle_pending()){
                handle_battle_policy(request, actions, result);
            }
            return true;
        }
        result.refreshes++;
    }
    return false;
}

bool expired(IsolatedHuntResult& result, const IsolatedHuntLimits& limits,
    uint64_t now, uint64_t hunt_started, uint64_t power_started
){
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

}

namespace{
struct HuntSetup{
    IsolatedHuntResult result;
    uint64_t power_started = 0;
};
HuntSetup setup_hunt(const IsolatedHuntRequest& request, const IsolatedHuntLimits& limits, IsolatedHuntActions& actions){
    if (!make_isolated_hunt_plan({request}).error.empty() ||
        (limits.ascent.enabled && !valid_isolated_ascent(limits.ascent)) ||
        limits.max_rounds == 0 || limits.max_empty_rounds == 0 || limits.duration_ms == 0 || limits.power_duration_ms == 0){
        return {{.stop = IsolatedHuntStop::INVALID_PLAN}};
    }
    if (!actions.reset_start(request)){
        return {failed_stage(request, actions, IsolatedHuntStop::START_FAILED)};
    }
    if (!actions.prepare_sandwich(request)){
        return {failed_stage(request, actions, IsolatedHuntStop::SETUP_FAILED)};
    }
    const uint64_t power_started = actions.now_ms();
    actions.set_deadlines(power_started + limits.power_duration_ms, UINT64_MAX);
    if (!actions.verify_meal_power(request)){
        return {failed_stage(request, actions, IsolatedHuntStop::POWER_UNVERIFIED)};
    }
    if (!actions.travel_to_anchor(request)){
        return {failed_stage(request, actions, IsolatedHuntStop::TRAVEL_FAILED)};
    }
    return {{.stop = IsolatedHuntStop::ROUND_LIMIT}, power_started};
}
}

bool can_leave_isolated_hunt(IsolatedHuntStop stop){
    return stop == IsolatedHuntStop::ROUND_LIMIT || stop == IsolatedHuntStop::TIME_LIMIT ||
        stop == IsolatedHuntStop::POWER_EXPIRED;
}

IsolatedHuntResult run_isolated_hunt(
    const IsolatedHuntRequest& request, const IsolatedHuntLimits& limits,
    IsolatedHuntActions& actions
){
    const auto setup = setup_hunt(request, limits, actions);
    if (setup.result.stop != IsolatedHuntStop::ROUND_LIMIT){ return setup.result; }
    const uint64_t power_started = setup.power_started;
    IsolatedHuntResult result{.stop = IsolatedHuntStop::ROUND_LIMIT};
    const uint64_t started = actions.now_ms();
    actions.set_deadlines(power_started + limits.power_duration_ms, started + limits.duration_ms);
    if (limits.ascent.enabled){
        return run_isolated_corridor(request, limits, actions, power_started, started);
    }
    uint8_t empty = 0;
    while (result.rounds < limits.max_rounds){
        if (expired(result, limits, actions.now_ms(), started, power_started)){
            return result;
        }
        const auto round = actions.clear_round(request);
        result.kills += round.kills;
        result.rounds++;
        if (round.event != IsolatedHuntEvent::CLEAR){
            if (handle_event(request, actions, result, round.event)){
                return result;
            }
            empty = 0;
            continue;
        }
        empty = round.kills == 0 ? empty + 1 : 0;
        const auto now = actions.now_ms();
        const bool allow_refresh = result.rounds < limits.max_rounds && now - started < limits.duration_ms &&
            now - power_started < limits.power_duration_ms;
        if (handle_empty_round(request, limits, actions, result, empty, allow_refresh)){
            return result;
        }
    }
    return result;
}

}
}
}
