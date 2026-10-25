#ifndef PokemonAutomation_PokemonSV_IsolatedHuntLoop_H
#define PokemonAutomation_PokemonSV_IsolatedHuntLoop_H

#include "PokemonSV_IsolatedHuntPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

enum class IsolatedMarkerState{ MISSING, PRESENT, ARRIVED };

enum class IsolatedHuntVerification{ MEAL_POWER, CENTRAL_PATH };
enum class IsolatedHuntEvent{ CLEAR, REFUSAL, BATTLE };
enum class IsolatedHuntBattleResult{ CONTINUE, STOP, FAILED };
enum class IsolatedHuntStop{ ROUND_LIMIT, INVALID_PLAN, START_FAILED, SETUP_FAILED, POWER_UNVERIFIED, TRAVEL_FAILED, UNRESOLVED_REFUSAL, BATTLE_POLICY, BATTLE_FAILED, TIME_LIMIT, NO_PROGRESS, REFRESH_FAILED, POWER_EXPIRED, MARKER_MISSING, REANCHOR_FAILED, CLEARING_TIMEOUT };

struct IsolatedHuntRound{
    uint64_t kills = 0;
    IsolatedHuntEvent event = IsolatedHuntEvent::CLEAR;
    bool budget_exhausted = false;
    bool clearing_busy = false;
};

struct IsolatedAscentLimits{
    bool enabled = false;
    uint16_t segments_per_ascent = 6;
    uint16_t segment_ms = 600;
    uint16_t clearing_wait_ms = 5000;
};

struct IsolatedHuntLimits{
    uint16_t max_rounds = 30;
    uint8_t max_empty_rounds = 3;
    uint64_t duration_ms = 120000;
    uint64_t power_duration_ms = 29 * 60 * 1000;
    IsolatedAscentLimits ascent;
    bool use_existing_power = false;
};

struct IsolatedHuntResult{
    IsolatedHuntStop stop;
    uint64_t kills = 0;
    uint16_t rounds = 0;
    uint16_t encounters = 0;
    uint16_t refusals = 0;
    uint16_t refreshes = 0;
};

class IsolatedHuntActions{
public:
    virtual ~IsolatedHuntActions() = default;
    virtual bool reset_start(const IsolatedHuntRequest& request) = 0;
    virtual bool prepare_sandwich(const IsolatedHuntRequest& request) = 0;
    virtual bool verify_meal_power(const IsolatedHuntRequest& request) = 0;
    // Also called after a handled battle; success must mean the route anchor is restored.
    virtual bool travel_to_anchor(const IsolatedHuntRequest& request) = 0;
    // Watch for battles and refusals throughout the bounded movement/observation window.
    virtual IsolatedHuntRound clear_round(const IsolatedHuntRequest& request) = 0;
    virtual bool refresh_spawns(const IsolatedHuntRequest& request) = 0;
    // True only after battle entry is confirmed, not merely after throwing the lead.
    virtual bool acquire_refused_target(const IsolatedHuntRequest& request) = 0;
    virtual IsolatedHuntBattleResult handle_battle(const IsolatedHuntRequest& request) = 0;
    virtual bool battle_pending() const{ return false; }
    virtual bool refusal_pending() const{ return false; }
    virtual IsolatedMarkerState marker_state(){ return IsolatedMarkerState::MISSING; }
    virtual bool realign_marker(){ return false; }
    virtual bool reset_ascent(const IsolatedHuntRequest& request, const char*){ return travel_to_anchor(request); }
    virtual IsolatedHuntRound advance_ascent(const IsolatedHuntRequest& request, const IsolatedAscentLimits&){
        return clear_round(request);
    }
    virtual void set_deadlines(uint64_t, uint64_t){}
    // Monotonic time; setup and initial travel are excluded from the hunt budget.
    virtual uint64_t now_ms() const = 0;
};

bool can_leave_isolated_hunt(IsolatedHuntStop stop);

IsolatedHuntResult run_isolated_hunt(
    const IsolatedHuntRequest& request, const IsolatedHuntLimits& limits,
    IsolatedHuntActions& actions
);

}
}
}
#endif
