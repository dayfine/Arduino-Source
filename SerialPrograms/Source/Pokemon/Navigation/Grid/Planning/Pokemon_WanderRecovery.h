/*  Wanderer Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared wait-policy state and pure decision helpers for recovering when a
 *  wandering NPC blocks a planned overworld walk (at a wall check, mid-leg,
 *  or on an unmoved first step).
 *
 */

#ifndef PokemonAutomation_Pokemon_WanderRecovery_H
#define PokemonAutomation_Pokemon_WanderRecovery_H

#include <cstddef>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Per-game policy for waiting on a wandering NPC to move out of the way.
struct WanderWaitPolicy{
    size_t max_waits = 0;
    Milliseconds wait_duration{0};
    //  True when the game probes a suspected wanderer at a wall stop by
    //  pressing A (e.g. BDSP) before waiting or replanning; false when the
    //  game uses a movement/screen check instead (e.g. FRLG).
    bool probe_with_a = false;
};

//  Tracks how many wanderer waits have been spent on the current step/leg.
struct WanderWaitState{
    size_t waits_used = 0;
};


//  Inputs for a wall-end wanderer check.
struct WallWanderInput{
    bool touches_wander_zone = false;
    bool probed_person = false;
};

//  Inputs for a mid-leg wanderer check.
struct MidLegWanderInput{
    bool has_wanderer_ahead = false;
    bool can_resume_remaining = false;
};


//  Decision for a wall-end check when a leg to a wall may have been stopped
//  by a wandering NPC instead of the wall.
enum class WallWanderDecision{
    NONE,
    WAIT_AND_REHOLD,
    REPLAN_BEFORE_ZONE,
};

//  Decision for a mid-leg stop when a wandering NPC blocks the tile ahead.
enum class MidLegWanderDecision{
    NONE,
    WAIT_AND_RESUME_REMAINING,
    WAIT_AND_REANCHOR,
};

//  Decision for a leg that moved nothing when a wandering NPC blocks the
//  first tile ahead.
enum class UnmovedWanderDecision{
    NONE,
    WAIT_AND_RETRY,
};


//  Decide how to handle a wall-end stop near a wander zone.
//  When `policy.probe_with_a` is true (BDSP), `input.probed_person` must be
//  true to treat the stop as a wanderer; exhausting waits returns
//  REPLAN_BEFORE_ZONE. When `policy.probe_with_a` is false (FRLG),
//  `input.probed_person` is ignored and exhausting waits returns NONE.
WallWanderDecision decide_wall_wander(
    const WanderWaitPolicy& policy,
    WanderWaitState& state,
    const WallWanderInput& input
);

//  Decide how to handle a mid-leg stop against a wandering NPC.
//  Returns WAIT_AND_RESUME_REMAINING when `input.can_resume_remaining` is true
//  (exact stopping tile known), WAIT_AND_REANCHOR otherwise, or NONE if
//  no wanderer is ahead or waits are exhausted.
MidLegWanderDecision decide_mid_leg_wander(
    const WanderWaitPolicy& policy,
    WanderWaitState& state,
    const MidLegWanderInput& input
);

//  Decide how to handle a leg that moved 0 tiles when a wandering NPC may be
//  blocking the first tile ahead.
UnmovedWanderDecision decide_unmoved_wander(
    const WanderWaitPolicy& policy,
    WanderWaitState& state,
    bool blocked_by_wanderer
);


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
