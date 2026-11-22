/*  Wanderer Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_WanderRecovery.h"

namespace PokemonAutomation{
namespace Pokemon{


WallWanderDecision decide_wall_wander(
    const WanderWaitPolicy& policy,
    WanderWaitState& state,
    const WallWanderInput& input
){
    if (!input.touches_wander_zone){
        return WallWanderDecision::NONE;
    }
    if (policy.probe_with_a && !input.probed_person){
        return WallWanderDecision::NONE;
    }
    if (state.waits_used < policy.max_waits){
        state.waits_used++;
        return WallWanderDecision::WAIT_AND_REHOLD;
    }
    if (policy.probe_with_a){
        return WallWanderDecision::REPLAN_BEFORE_ZONE;
    }
    return WallWanderDecision::NONE;
}


MidLegWanderDecision decide_mid_leg_wander(
    const WanderWaitPolicy& policy,
    WanderWaitState& state,
    const MidLegWanderInput& input
){
    if (!input.has_wanderer_ahead || state.waits_used >= policy.max_waits){
        return MidLegWanderDecision::NONE;
    }
    state.waits_used++;
    return input.can_resume_remaining
        ? MidLegWanderDecision::WAIT_AND_RESUME_REMAINING
        : MidLegWanderDecision::WAIT_AND_REANCHOR;
}


UnmovedWanderDecision decide_unmoved_wander(
    const WanderWaitPolicy& policy,
    WanderWaitState& state,
    bool blocked_by_wanderer
){
    if (!blocked_by_wanderer || state.waits_used >= policy.max_waits){
        return UnmovedWanderDecision::NONE;
    }
    state.waits_used++;
    return UnmovedWanderDecision::WAIT_AND_RETRY;
}


} // namespace Pokemon
} // namespace PokemonAutomation
